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
  scene_intersect(ss.ray);
  if (!scene_intersect(ss.ray))
    return vec4(0);

  // Get info about the intersected surface; if an emitter was intersected, return early
  SurfaceInfo si = get_surface_info(ss.ray);
  if (!is_valid(si) || !is_object(si))
    return vec4(0);

  // Output values
  vec3  wo  = vec3(0);
  float f   = 0.f;
  float pdf = 0.f;

  // BRDF sampling and computation
  while (pdf <= 0.f) { // To simplify debug, we forcibly loop a valid sample is found
    // Sample cosine hemisphere for output
    // wo  = square_to_cos_hemisphere(next_2d(state));
    // pdf = square_to_cos_hemisphere_pdf(wo);

    // Reflect specularly
    wo  = vec3(-si.wi.xy, si.wi.z);
    pdf = 1.f;

    // BRDF parameters
    float f_0     = 0.3f;
    float alpha   = 0.1f;
    float alpha_2 = sdot(alpha);

    // BRDF throughput 
    vec3 wh = normalize(si.wi + wo);

    if (record_get_object(si.data) == 1) { // Intersect with first sphere
      // GGX normal distribution function
      float ggx = (alpha_2 * cos_theta(wh) - cos_theta(wh)) * cos_theta(wh) + 1.f;
      ggx = alpha_2 / (ggx * ggx);

      float g1_masking = cos_theta(wo)    
                       * sqrt((cos_theta(si.wi) - alpha_2 * cos_theta(si.wi)) * cos_theta(si.wi) + alpha_2);
      float g1_shadow  = cos_theta(si.wi) 
                       * sqrt((cos_theta(wo)    - alpha_2 * cos_theta(wo))    * cos_theta(wo)    + alpha_2);
      float g2 = 0.5f / (g1_masking + g1_shadow);

      float fresnel = fresnel_schlick(f_0, 1.f, cos_theta(si.wi));

      // D(N) = 1 / (pi * (...))
      f = /* M_PI_INV * fresnel * ggx * */ g2;
    } else if (record_get_object(si.data) == 2) { // Intersect with second sphere
      // GGX normal distribution function
      // Initial
      // float ggx = sdot(wh.xy) / alpha_2 + sdot(wh.z);
      // ggx = 1.f / (alpha_2 * (ggx * ggx));
      // Rewrite confirmation; it really is the same as christoph's
      float ggx = (alpha_2 * wh.z - wh.z) * wh.z + 1.f;
      ggx = alpha_2 / (ggx * ggx);

      float lambda_in
        = -.5f + .5f * sqrt(
          1.f + alpha_2 / (si.wi.z * si.wi.z) - alpha_2
        );
      float lambda_out
        = -.5f + .5f * sqrt(
          1.f + alpha_2 / (wo.z * wo.z) - alpha_2
        );
      float g1_in  = 1.f / (1.f + lambda_in);
      float g1_out = 1.f / (1.f + lambda_out);
      float g2_ = g1_in * g1_out;
      float g2 = g2_ / (4.f * cos_theta(wo) * cos_theta(si.wi));

      float fresnel = schlick_fresnel(vec4(f_0), vec4(1), cos_theta(si.wi)).x;

      // D(N) = 1 / (pi * (...))
      f = /* M_PI_INV * fresnel * ggx * */ g2;
    } else { // Intersect with background plane, or other object
      f = 0.f;
      pdf = 1.f;
    }
  }

  // Adjust for spectral intergration by doing 4 / n
  float L = f * cos_theta(wo) / pdf;
  return vec4(L * 4.f / float(wavelength_samples));
}

vec4 Li(in SensorSample ss, in SamplerState state, inout float alpha) {
  // Initialize path store if requested for path queries
  path_query_initialize(pt);
  
  // Path throughput information; we track 4 wavelengths simultaneously
  vec4 S    = vec4(0.f);
  vec4 beta = vec4(1.f / ss.pdfs);

  // Prior brdf sample, default-initialized, kept for multiple importance sampling
  BRDFSample bs; 
  bs.pdf      = 1;
  bs.is_delta = true;
  
  // Iterate up to maximum depth
  for (uint depth = 0; depth < max_depth; ++depth) {
    // Ray-trace first. Then, if no surface is intersected by the ray, 
    // add contribution of environment map, and terminate the current path
    if (!scene_intersect(ss.ray)) {
      if (scene_has_envm_emitter()) {
        float em_pdf = (depth == 0 || bs.is_delta)  
                     ? 0.f 
                     : pdf_env_emitter(bs.wo);
            
        vec4 s = beta
               * eval_env_emitter(ss.wvls)
               * mis_power(bs.pdf, em_pdf);

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
      PositionSample ps     = get_position_sample(si);
      float          em_pdf = bs.is_delta ? 0.f : pdf_emitters(ps);

      // No division by sample density, as this is incorporated in path throughput
      vec4 s = beta                       // throughput 
             * eval_emitter(ps, ss.wvls)  // emitted value
             * mis_power(bs.pdf, em_pdf); // mis weight

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
    
    // Direct illumination sampling
    {
      // Generate position sample on emitter
      // Importance sample emitter position
      PositionSample pe     = sample_emitters(si, next_3d(state));
      vec3           wo     = to_local(si, pe.d);
      float          bs_pdf = bs.pdf * pdf_brdf(brdf, si, wo);
      
      // If the sample position has potential throughput, 
      // evaluate a ray towards the position and add contribution to output
      if (pe.pdf != 0.f && bs_pdf != 0.f && cos_theta(wo) > 0.f) {
        // Avoid diracs when calculating mis weight
        float mis_weight = pe.is_delta 
                         ? 1.f / pe.pdf 
                         : mis_power(pe.pdf, bs_pdf) / pe.pdf;
        
        // Test for any hit closer than sample position
        if (!scene_intersect_any(ray_towards_point(si, pe.p))) {
          // Assemble path throughput
          vec4 s = beta                       // Throughput
                 * eval_brdf(brdf, si, wo)    // brdf value
                 * abs(cos_theta(wo))         // cosine attenuation
                 * eval_emitter(pe, ss.wvls)  // emitted value
                 * mis_weight;                // mis weight

          // Store current path query if requested
          path_query_finalize_emitter(pt, pe, s, ss.wvls);

          // Add to output radiance
          S += s;
        }
      }
    }

    // BRDF sampling
    {
      // Importance sample brdf direction
      bs = sample_brdf(brdf, next_3d(state), si);
      if (bs.pdf == 0.f)
        break;
      
      // Update throughput
      beta *= bs.f                  // brdf value
            * abs(cos_theta(bs.wo)) // cosine attenuation
            / bs.pdf;               // sample density
      
      // Early exit on zero throughput
      if (all(is_zero(beta)))
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