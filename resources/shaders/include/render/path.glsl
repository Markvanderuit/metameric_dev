#ifndef RENDER_PATH_GLSL_GUARD
#define RENDER_PATH_GLSL_GUARD

#include <sampler/uniform.glsl>
#include <render/scene.glsl>
#include <render/surface.glsl>
#include <render/sensor.glsl>
#include <render/brdf.glsl>
#include <render/detail/path_query.glsl>

vec4 Li_debug(in SensorSample ss, in SamplerState state) {
  // Ray-trace first. If no surface is intersected by the ray, return early
  if (!scene_intersect(ss.ray))
    return vec4(1);

  // Get info about the intersected surface; if an emitter was intersected, return early
  SurfaceInfo si = get_surface_info(ss.ray);
  if (!is_valid(si) || !is_object(si))
    return vec4(0);
    
  return vec4(si.t, si.t, si.t, 1);
}

vec4 Li(in SensorSample ss, in SamplerState state, out float alpha) {
  // Initialize path store if requested for path queries
  path_query_initialize(pt);
  
  // Path throughput information; we track 4 wavelengths simultaneously
  vec4  Li   = vec4(0);           // Accumulated spectrum
  vec4  Beta = vec4(1 / ss.pdfs); // Path throughput over density

  // Prior brdf sample data, default-initialized, kept for NEE and MIS
  float bs_pdf         = 1.f;
  bool  bs_is_delta    = true;
  bool  bs_is_spectral = false;

  // Iterate up to maximum depth
  for (uint depth = 0; ; ++depth) {
    guard_break(max_depth == 0 || depth < max_depth);

    // Ray-trace first. Then, if no surface is intersected by the ray, 
    // add contribution of environment map and terminate the current path
    if (!scene_intersect(ss.ray)) {
      if (scene_has_envm_emitter()) {
        float em_pdf = bs_is_delta ? 0.f : pdf_env_emitter(ss.ray.d, ss.wvls);
        
        // No division by sample density, as this is incorporated in path throughput
        vec4 s = Beta                       // throughput
               * eval_env_emitter(ss.wvls)  // emitted value
               * mis_power(bs_pdf, em_pdf); // mis weight

        // Store current path query if requested
        path_query_finalize_envmap(pt, s, ss.wvls);

        // Add to output radiance
        Li += s;
      }
      
      // Output 0 alpha on initial ray miss
      alpha = depth > 0 ? 1.f : 0.f;
      break;
    } else {
      alpha = 1.f;
    }

    // Get info about the intersected surface
    SurfaceInfo si = get_surface_info(ss.ray);

    // Store the next vertex to path query if requested
    path_query_extend(pt, ss.ray);

    // If an emissive object is hit, add its contribution to the 
    // current path, and then terminate said path
    if (is_emitter(si)) {
      float em_pdf = bs_is_delta ? 0.f : pdf_emitters(si, ss.wvls);

      // No division by sample density, as this is incorporated in path throughput
      vec4 s = Beta                       // throughput 
             * eval_emitter(si, ss.wvls)  // emitted value
             * mis_power(bs_pdf, em_pdf); // mis weight

      // Store current path query if requested
      path_query_finalize_direct(pt, s, ss.wvls);

      // Add to output radiance and terminate path
      Li += s;
      break;
    }

    // If maximum path depth was reached at this point, terminate
    if (depth == max_depth - 1)
      break;

    // Construct the underlying BRDF at the intersected surface
    BRDFInfo brdf = get_brdf(si, ss.wvls, next_2d(state));
    
    // Emitter sampling
    {
      // Generate directional sample towards emitter
      EmitterSample es = sample_emitters(si, ss.wvls, next_3d(state));
      
      // Exitant direction in local frame
      vec3 wo = to_local(si, es.ray.d);
      
      // Sample density of brdf for this sample
      float brdf_pdf = bs_pdf * pdf_brdf(brdf, si, wo);

      // If the sample position has potential throughput, 
      // evaluate a ray towards the position and add contribution to output
      if (es.pdf > 0 && brdf_pdf > 0) {
        // Test for any hit closer than sample position
        if (!scene_intersect_any(es.ray)) {
          // Avoid diracs when calculating mis weight
          float mis_weight = es.is_delta ? 1.f : mis_power(es.pdf, brdf_pdf);

          // Assemble path throughput
          vec4 s = Beta                    // current path throughput
                 * eval_brdf(brdf, si, wo) // brdf response
                 * cos_theta(wo)           // cosine attenuation
                 * es.L                    // emitter response
                 * mis_weight              // mis weight
                 / es.pdf;                 // sample density

          // Store current path query if requested
          path_query_finalize_emitter(pt, es, s, ss.wvls);

          // Add to output radiance
          Li += s;
        }
      }
    }

    // BRDF sampling
    {
      // Importance sample brdf direction
      BRDFSample bs = sample_brdf(brdf, next_3d(state), si);

      // Early exit on zero brdf density
      if (bs.pdf == 0.f)
        break;
      
      // Update throughput, sample density
      Beta *= eval_brdf(brdf, si, bs.wo) // brdf throughput
            * abs(cos_theta(bs.wo))      // cosine attenuation
            / bs.pdf;

      // Store last brdf sample information for NEE
      bs_pdf      = bs.pdf;
      bs_is_delta = bs.is_delta;

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