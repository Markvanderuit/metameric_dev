#ifndef RENDER_PATH_GLSL_GUARD
#define RENDER_PATH_GLSL_GUARD

#include <sampler/uniform.glsl>
#include <render/ray.glsl>
#include <render/scene.glsl>
#include <render/surface.glsl>
#include <render/sensor.glsl>

// Macros for enabling/disabling path tracking
#ifdef ENABLE_PATH_TRACKING
#define path_query_initialize(pt) Path pt; { pt.path_depth = 0; }
void path_query_extend(inout Path pt, in Ray r) {
  pt.data[pt.path_depth++] = PathVertex(ray_get_position(r), r.data);
}
void path_query_finalize_direct(in Path pt, in vec4 L, in vec4 wvls) {
  pt.wvls = wvls;
  pt.L    = L;
  set_path(pt, get_next_path_id());
}
void path_query_finalize_emitter(in Path pt, in PositionSample r, in vec4 L, in vec4 wvls) {
  pt.data[pt.path_depth++] = PathVertex(r.p, r.data);
  pt.wvls = wvls;
  pt.L    = L;
  set_path(pt, get_next_path_id());
}
void path_query_finalize_envmap(in Path pt, in vec4 L, in vec4 wvls) {
  // pt.data[pt.path_depth++] = PathVertex(r.p, r.data);
  pt.wvls = wvls;
  pt.L    = L;
  set_path(pt, get_next_path_id());
}
#else  
#define path_query_initialize(pt)                     {}
#define path_query_extend(path, vt)                   {}
#define path_query_finalize_direct(path, L, wvls)     {}
#define path_query_finalize_emitter(path, r, L, wvls) {}
#define path_query_finalize_envmap(path, L, wvls)     {}
#endif

vec4 Li_debug(in SensorSample ss, in SamplerState state) {
  // If the ray misses, terminate current path
  scene_intersect(ss.ray);
  if (!scene_intersect(ss.ray))
    return vec4(0);
  
  // uint n = scene_emitter_count() + scene_object_count();
 /*  uint i = ray.data; // record_get_object(ray.data);
  uint n = 1024;

  float s = float(i) / float(n);
  return vec4(vec3(s), 1); */

  // return vec4(debug_colors[record_get_object(ray.data) % 8], 1);
  // return vec4(1, 0, s, 1);

  SurfaceInfo si = get_surface_info(ss.ray);
  if (!is_valid(si) || !is_object(si))
    return vec4(0);


  // float f = _G1(si.n, sdot(0.1));


  return vec4(si.sh.n * 4.f / float(wavelength_samples), 1);

  // // Hope this is the right one; should be normalized d65
  // vec4 d65_n = scene_illuminant(1, wvls);

  // Sample BRDF albedo at position, integrate, and return color
  // BRDFInfo brdf = get_brdf(si, ss.wvls);
  // return brdf.r / wvl_pdfs;

  // BRDFSample bs = sample_brdf(brdf, next_3d(state), si);
  // if (bs.pdf == 0.f)
  //   return vec4(0);
  // return bs.f 
  //     * cos_theta(bs.wo)
  //     / (ss.pdfs * bs.pdf);
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
             * eval_emitter(ps, ss.wvls)     // emitted value
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
    BRDFInfo brdf = get_brdf(si, ss.wvls);
    
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
          vec4 s = beta                    // Throughput
                 * eval_brdf(brdf, si, wo) // brdf value
                 * abs(cos_theta(wo))      // cosine attenuation
                 * eval_emitter(pe, ss.wvls)  // emitted value
                 * mis_weight;             // mis weight

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