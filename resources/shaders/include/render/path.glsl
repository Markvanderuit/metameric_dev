#ifndef RENDER_PATH_GLSL_GUARD
#define RENDER_PATH_GLSL_GUARD

#include <sampler/uniform.glsl>
#include <render/scene.glsl>
#include <render/interaction.glsl>
#include <render/sensor.glsl>
#include <render/brdf.glsl>
#include <render/detail/path_query.glsl>

vec4 Li_debug(in SensorSample ss, in SamplerState state) {
  // Ray-trace first. If no surface is intersected by the ray, return early
  if (!scene_intersect(ss.ray))
    return vec4(0);

  // Get info about the intersected surface; if an emitter was intersected, return early
  Interaction si = get_interaction(ss.ray);
  if (!is_object(si))
    return vec4(1);

  // Construct the underlying BRDF at the intersected surface
  BRDF brdf = get_brdf(si, ss.wvls, next_2d(state));
  
  return vec4(mulsign(si.wi, cos_theta(si.wi)), 1);
}

vec4 Li(in SensorSample ss, in SamplerState state, out float alpha) {
  // Initialize path store if requested for path queries
  path_query_initialize(pt);
  
  // Path throughput information; we track 4 wavelengths simultaneously
  vec4 Li   = vec4(0);           // Accumulated spectrum
  vec4 Beta = vec4(1 / ss.pdfs); // Path throughput over density

  // Prior brdf sample data, default-initialized, kept for NEE and MIS
  float prev_bs_pdf         = 1.f;
  bool  prev_bs_is_delta    = true;
  bool  bs_is_spectral = false;

  // Iterate up to maximum depth
  for (uint depth = 0; /* ... */ ; ++depth) {
    guard_break(max_depth == 0 || depth < max_depth);

    // Ray-trace against scene first
    if (!scene_intersect(ss.ray)) {
      // Output 0 alpha on initial ray miss
      alpha = depth > 0 ? 1.f : 0.f;
    } else {
      // Store the next vertex to path query if requested
      path_query_extend(pt, ss.ray);
      alpha = 1.f;
    }

    // Get info about the intersected surface or lack thereof
    Interaction si = get_interaction(ss.ray);
    guard_break(is_valid(si));

    // If an emissive object or envmap is hit, add its contribution to the 
    // current path, and then terminate
    if (is_emitter(si)) {
      float em_pdf = prev_bs_is_delta ? 0.f : pdf_emitter(si);

      // No division by sample density, as this is incorporated in path throughput
      vec4 s = Beta                                      // path throughput 
             * eval_emitter(si, ss.wvls, next_2d(state)) // emitter contribution
             * mis_power(prev_bs_pdf, em_pdf);           // mis weight

      // Store current path query if requested
      path_query_finalize_direct(pt, s, ss.wvls);

      // Add to output radiance and terminate path
      Li += s;
      break;
    }

    // Construct the underlying BRDF at the intersected surface
    BRDF brdf = get_brdf(si, ss.wvls, next_2d(state));
    
    // Emitter sampling
    {
      // Generate directional sample towards emitter
      EmitterSample es = sample_emitter(si, ss.wvls, next_3d(state));
      
      // BRDF sample density for exitant direction in local frame
      vec3  wo     = to_local(si, es.ray.d);
      float bs_pdf = pdf_brdf(brdf, si, wo, ss.wvls);

      // If the sample has throughput, test for occluder closer 
      // than sample position and add contribution
      if (es.pdf > 0 && bs_pdf > 0 && !scene_intersect_any(es.ray)) {
        // Avoid diracs when calculating mis weight
        float mis_weight = es.is_delta ? 1.f : mis_power(es.pdf, bs_pdf);

        // Assemble path throughput
        vec4 s = Beta                                      // path throughput
               * eval_brdf(brdf, si, wo, ss.wvls)          // brdf response
               * cos_theta(wo)                             // cosine attenuation
               * eval_emitter(es, ss.wvls, next_2d(state)) // emitter contribution
               * mis_weight                                // mis weight
               / es.pdf;                                   // sample density

        // Store current path query if requested
        path_query_finalize_emitter(pt, es, s, ss.wvls);

        // Add to output radiances
        Li += s;
      }
    }

    // BRDF sampling
    {
      // Importance sample brdf direction
      BRDFSample bs = sample_brdf(brdf, next_3d(state), si, ss.wvls);

      // Early exit on zero brdf density
      if (bs.pdf == 0.f)
        break;
      
      // Update throughput, sample density
      Beta *= eval_brdf(brdf, si, bs.wo, ss.wvls) // brdf throughput
            * abs(cos_theta(bs.wo))               // cosine attenuation
            / bs.pdf;

      // Retain last brdf density for direct emitter MIS weight
      prev_bs_pdf      = bs.pdf;
      prev_bs_is_delta = bs.is_delta;

      // Handle wavelength-dependence in the BRDF by terminating secondary wavelengths
      if (!bs_is_spectral && bs.is_spectral) {
        bs_is_spectral = true;
        Beta *= vec4(4, 0, 0, 0);
      }

      // Generate the next ray to trace through the scene
      ss.ray = ray_towards_direction(si, to_world(si, bs.wo));
    }

    // Russian Roulette
    if (rr_depth != 0 && depth >= rr_depth) {
      float q = min(0.95, hmax(Beta));
      guard_break(next_1d(state) < q);
      Beta /= q;
    }
  } // for (uint depth)

  return Li;
}

#endif // RENDER_PATH_GLSL_GUARD