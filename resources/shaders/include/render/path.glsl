#ifndef RENDER_PATH_GLSL_GUARD
#define RENDER_PATH_GLSL_GUARD

#include <sampler/uniform.glsl>
#include <render/ray.glsl>
#include <render/scene.glsl>
#include <render/surface.glsl>
#include <render/sensor.glsl>

// Macros for enabling/disabling path tracking
#ifdef ENABLE_PATH_TRACKING
#define path_initialize(pt) Path pt; { pt.path_depth = 0; }
void path_extend(inout Path pt, in Ray r) {
  pt.data[pt.path_depth++] = PathVertex(ray_get_position(r), r.data);
}
void path_finalize_direct(in Path pt, in vec4 L, in vec4 wvls) {
  pt.wvls = wvls;
  pt.L    = L;
  set_path(pt, get_next_path_id());
}
void path_finalize_emitter(in Path pt, in PositionSample r, in vec4 L, in vec4 wvls) {
  pt.data[pt.path_depth++] = PathVertex(r.p, r.data);
  pt.wvls = wvls;
  pt.L    = L;
  set_path(pt, get_next_path_id());
}
void path_finalize_envmap(in Path pt, in vec4 L, in vec4 wvls) {
  // pt.data[pt.path_depth++] = PathVertex(r.p, r.data);
  pt.wvls = wvls;
  pt.L    = L;
  set_path(pt, get_next_path_id());
}
#else  
#define path_initialize(pt)                     {}
#define path_extend(path, vt)                   {}
#define path_finalize_direct(path, L, wvls)     {}
#define path_finalize_emitter(path, r, L, wvls) {}
#define path_finalize_envmap(path, L, wvls)  {}
#endif

const vec3 debug_colors[8] = vec3[8](
  vec3(27,158,119) / 255.f,
  vec3(217,95,2) / 255.f,
  vec3(117,112,179) / 255.f,
  vec3(231,41,138) / 255.f,
  vec3(102,166,30) / 255.f,
  vec3(230,171,2) / 255.f,
  vec3(166,118,29) / 255.f,
  vec3(102,102,102) / 255.f
);

vec4 Li_debug(in Ray ray, in vec4 wvls, in vec4 wvl_pdfs, in SamplerState state) {
  // If the ray misses, terminate current path
  scene_intersect(ray);
  if (!scene_intersect(ray))
    return vec4(0);
  
  // uint n = scene_emitter_count() + scene_object_count();
 /*  uint i = ray.data; // record_get_object(ray.data);
  uint n = 1024;

  float s = float(i) / float(n);
  return vec4(vec3(s), 1); */

  // return vec4(debug_colors[record_get_object(ray.data) % 8], 1);
  // return vec4(1, 0, s, 1);

  SurfaceInfo si = get_surface_info(ray);
  if (!is_valid(si) || !is_object(si))
    return vec4(0);

  // // Hope this is the right one; should be normalized d65
  // vec4 d65_n = scene_illuminant(1, wvls);

  // Sample BRDF albedo at position, integrate, and return color
  BRDFInfo brdf = get_brdf(si, wvls);
  // return brdf.r / wvl_pdfs;

  BRDFSample bs = sample_brdf(brdf, next_3d(state), si);
  if (bs.pdf == 0.f)
    return vec4(0);
  return bs.f 
      * cos_theta(bs.wo)
      / (wvl_pdfs * bs.pdf);
}

vec4 Li(in Ray ray, in vec4 wvls, in vec4 wvl_pdfs, in SamplerState state, inout float alpha) {
  // Initialize path store if requested
  path_initialize(pt);
  
  // Path throughput information; we track 4 wavelengths simultaneously
  vec4  S    = vec4(0.f);
  vec4  beta = vec4(1.f / wvl_pdfs);

  // Last brdf sample, default-initialized
  BRDFSample bs; 
  bs.pdf      = 1.f;
  bs.is_delta = true;
  
  // Iterate up to maximum depth
  for (uint depth = 0; depth < max_depth; ++depth) {
    // If no surface object is visible, add contribution of envmap, 
    // then terminate current path
    if (!scene_intersect(ray)) {
      if (scene_has_envm_emitter()) {
        float em_pdf = (depth == 0 || bs.is_delta)  
                     ? 1.f 
                     : pdf_env_emitter(bs.wo);
        float mis_weight = depth == 0
                         ? 1.f
                         : mis_power(bs.pdf, em_pdf);
        
        vec4 s = beta
               * eval_env_emitter(wvls)
               * mis_weight;

        // Store current path if requested
        path_finalize_envmap(pt, s, wvls);

        // Add to output radiance
        S += s;
      }
      
      // Output 0 alpha on initial ray miss
      alpha = depth > 0 ? 1.f : 0.f;
      
      break;
    }

    // Get info about next path vertex
    SurfaceInfo si = get_surface_info(ray);

    // Store next vertex to path if requested
    path_extend(pt, ray);

    // If an emitter is hit directly, add contribution to path, 
    // then terminate current path
    if (is_emitter(si)) {
      PositionSample ps     = get_position_sample(si);
      float          em_pdf = bs.is_delta ? 0.f : pdf_emitters(ps);

      // No division by sample density, as this is incorporated in path throughput
      vec4 s = beta                       // throughput 
             * eval_emitter(ps, wvls)     // emitted value
             * mis_power(bs.pdf, em_pdf); // mis weight

      // Store current path if requested
      path_finalize_direct(pt, s, wvls);

      // Add to output radiance and terminate path
      S += s;
      break;
    }

    // Terminate at maximum path length
    if (depth == max_depth - 1)
      break;

    // Sample BRDF at position
    BRDFInfo brdf = get_brdf(si, wvls);
    
    // Direct illumination sampling;
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
        Ray ray = ray_towards_point(si, pe.p);
        if (!scene_intersect_any(ray)) {
          // Assemble path throughput
          vec4 s = beta                    // Throughput
                 * eval_brdf(brdf, si, wo) // brdf value
                 * abs(cos_theta(wo))      // cosine attenuation
                 * eval_emitter(pe, wvls)  // emitted value
                 * mis_weight;             // mis weight

          // Store current path if requested
          path_finalize_emitter(pt, pe, s, wvls);

          // Add to output radiance
          S += s;
        }
      }
    }

    // BRDF sampling; 
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

      // Generate next scene ray
      ray = ray_towards_direction(si, to_world(si, bs.wo));
    }

    // TODO RR goes here
    // ...
  } // for (uint depth)

  return S;
}

vec4 Li(in SensorSample sensor_sample, in SamplerState state, inout float alpha) {
  return Li(sensor_sample.ray, sensor_sample.wvls, sensor_sample.pdfs, state, alpha);
}

vec4 Li_debug(in SensorSample sensor_sample, in SamplerState state) {
  return Li_debug(sensor_sample.ray, sensor_sample.wvls, sensor_sample.pdfs, state);
}

#endif // RENDER_PATH_GLSL_GUARD