#ifndef RENDER_PATH_GLSL_GUARD
#define RENDER_PATH_GLSL_GUARD

#include <sampler/uniform.glsl>
#include <render/ray.glsl>
#include <render/scene.glsl>
#include <render/surface.glsl>
#include <render/sensor.glsl>
#include <render/detail/path_query.glsl>

float fresnel_schlick(float f_0, float f_90, float lambert) {
	float flip_1 = 1.0 - lambert;
	float flip_2 = flip_1 * flip_1;
	float flip_5 = flip_2 * flip_1 * flip_2;
	return flip_5 * (f_90 - f_0) + f_0;
}

vec4 Li_debug(in SensorSample ss, in SamplerState state) {
  // Ray-trace first. If no surface is intersected by the ray, return early
  if (!scene_intersect(ss.ray))
    return vec4(1);

  // Get info about the intersected surface; if an emitter was intersected, return early
  SurfaceInfo si = get_surface_info(ss.ray);
  if (!is_valid(si) || !is_object(si))
    return vec4(0);
  
  if (si.tx == vec2(0))
    return vec4(1, 0, 0, 1);
  else
    return vec4(si.tx * 0.05, 0, 1);

  /* BRDFInfo   brdf = get_brdf(si, ss.wvls, next_2d(state));
  BRDFSample bs   = sample_brdf(brdf, next_3d(state), si);
  vec4 f = eval_brdf(brdf, si, bs.wo);
  vec4 c = f * abs(cos_theta(si.wi)) / pdf_brdf_diffuse(brdf, si, bs.wo);
  if (bs.pdf == 0.f)
    c = vec4(0, 0, 0, 0);

  // Generate position sample on emitter, with ray towards sample
  EmitterSample es = sample_emitters(si, ss.wvls, next_3d(state));
  // Exitant direction in local frame
  // vec3 wo = to_local(si, es.ray.d);
  c = vec4(abs(es.ray.d), 1);

  // Adjust for spectral integration by doing 4 / n
  return c; */
}

vec4 Li(in SensorSample ss, in SamplerState state, inout float alpha) {
  // Initialize path store if requested for path queries
  path_query_initialize(pt);
  
  // Path throughput information; we track 4 wavelengths simultaneously
  vec4 S    = vec4(0.f);
  vec4 beta = vec4(1.f / ss.pdfs);

  // Prior brdf sample data, default-initialized, kept for multiple importance sampling
  float bs_pdf      = 1.f;
  bool  bs_is_delta = true;
  
  // Iterate up to maximum depth
  for (uint depth = 0; depth < max_depth; ++depth) {
    // Ray-trace first. Then, if no surface is intersected by the ray, 
    // add contribution of environment map, and terminate the current path
    if (!scene_intersect(ss.ray)) {
      if (scene_has_envm_emitter()) {
        float em_pdf = bs_is_delta ? 0.f : pdf_env_emitter(ss.ray.d, ss.wvls);
        
        // No division by sample density, as this is incorporated in path throughput
        vec4 s = beta                       // throughput
               * eval_env_emitter(ss.wvls)  // emitted value
               * mis_power(bs_pdf, em_pdf); // mis weight

        // Store current path query if requested
        path_query_finalize_envmap(pt, s, ss.wvls);

        // Add to output radiance
        S += s;
      }
      
      // Output 0 alpha on initial ray miss
      alpha = depth > 0 ? 1.f : 0.f;
      
      break;
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
      vec4 s = beta                       // throughput 
             * eval_emitter(si, ss.wvls)  // emitted value
             * mis_power(bs_pdf, em_pdf); // mis weight

      // Store current path query if requested
      path_query_finalize_direct(pt, s, ss.wvls);

      // Add to output radiance and terminate path
      S += s;
      
      break;
    }

    // If maximum path depth was reached at this point, terminate
    if (depth == max_depth - 1)
      break;

    // Construct the underlying BRDF at the intersected surface
    BRDFInfo brdf = get_brdf(si, ss.wvls, next_2d(state));
    
    // Emitter sampling
    {
      // Generate position sample on emitter, with ray towards sample
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
          vec4 s = beta                    // current path throughput
                 * es.L                    // emitter response
                 * eval_brdf(brdf, si, wo) // brdf response
                 * cos_theta(wo)           // cosine attenuation
                 * mis_weight              // mis weight
                 / es.pdf;                 // sample density

          // Store current path query if requested
          path_query_finalize_emitter(pt, es, s, ss.wvls);

          // Add to output radiance
          S += s;
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
      
      // Update throughput
      beta *= eval_brdf(brdf, si, bs.wo) // brdf throughput
            * abs(cos_theta(bs.wo))      // cosine attenuation
            / bs.pdf;                    // sample density

      // Update relevant sample information
      bs_pdf      = bs.pdf;
      bs_is_delta = bs.is_delta;
      
      // Early exit on zero throughput
      if (beta == vec4(0))
        break;

      // Generate the next ray to trace through the scene
      ss.ray = ray_towards_direction(si, to_world(si, bs.wo));
    }

    // TODO RR goes here
    // ...
  } // for (uint depth)

  return S;
}

#endif // RENDER_PATH_GLSL_GUARD