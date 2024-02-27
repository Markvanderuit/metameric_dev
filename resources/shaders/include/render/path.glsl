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
#else  
#define path_initialize(pt)                     {}
#define path_extend(path, vt)                   {}
#define path_finalize_direct(path, L, wvls)     {}
#define path_finalize_emitter(path, r, L, wvls) {}
#endif

vec4 Li_debug(in Ray ray, in vec4 wvls, in vec4 wvl_pdfs, in SamplerState state) {
  // If the ray misses, terminate current path
  if (!scene_intersect(ray))
    return vec4(0);

  SurfaceInfo si = get_surface_info(ray);
  if (!is_valid(si) || !is_object(si))
    return vec4(0);

  // Sample BRDF at position
  BRDFInfo brdf = get_brdf(si, wvls);

  return brdf.r;
}

vec4 Li(in Ray ray, in vec4 wvls, in vec4 wvl_pdfs, in SamplerState state) {
  // Initialize path store if requested
  path_initialize(pt);
  
  // Path throughput information; we track 4 wavelengths simultaneously
  vec4  S               = vec4(0.f);
  vec4  beta            = vec4(1.f / wvl_pdfs);
  float prev_bsdf_pdf   = 1.f;
  bool  prev_bsdf_delta = true;
  
  // Iterate up to maximum depth
  for (uint depth = 0; depth < max_depth; ++depth) {
    // If the ray misses, terminate current path
    if (!scene_intersect(ray))
      break;

    // If no surface object is visible, terminate current path
    SurfaceInfo si = get_surface_info(ray);
    if (!is_valid(si))
      break;

    // Store next vertex to path if requested
    path_extend(pt, ray);

    // If an emissive object is hit directly, add contribution to path
    if (is_emitter(si)) {
      PositionSample ps       = get_position_sample(si);
      float          emtr_pdf = prev_bsdf_delta ? 0.f : pdf_emitters(ps);

      // No division by sample density, as this is incorporated in path throughput
      vec4 s = beta                                // throughput 
             * eval_emitter(ps, wvls)              // emitted value
             * mis_power(prev_bsdf_pdf, emtr_pdf); // mis weight

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
      PositionSample ps       = sample_emitters(si, next_3d(state));
      vec3           wo       = to_local(si, ps.d);
      float          bsdf_pdf = prev_bsdf_pdf * pdf_brdf(brdf, si, wo);
      
      // If the sample position has potential throughput, 
      // evaluate a ray towards the position and add contribution to output
      if (ps.pdf != 0.f && bsdf_pdf != 0.f && cos_theta(wo) > 0.f) {
        Ray ray = ray_towards_point(si, ps.p);
        if (!scene_intersect_any(ray)) {
          vec4 s = beta                         // Throughput
                 * eval_brdf(brdf, si, wo)      // brdf value
                 * cos_theta(wo)                // cosine attenuation
                 * eval_emitter(ps, wvls)       // emitted value
                 * mis_power(ps.pdf, bsdf_pdf)  // mis weight
                 / ps.pdf;                      // sample density

          // Store current path if requested
          path_finalize_emitter(pt, ps, s, wvls);

          // Add to output radiance
          S += s;
        }
      }
    }

    // BRDF sampling; 
    {
      // Importance sample brdf direction
      BRDFSample bs = sample_brdf(brdf, next_2d(state), si);
      if (bs.pdf == 0.f)
        break;
      
      // Update throughput
      beta *= bs.f              // brdf value
            * cos_theta(bs.wo)  // cosine attenuation
            / bs.pdf;           // sample density
      
      // Early exit on zero throughput
      if (all(is_zero(beta)))
        break;

      // Store previous BRDF information for MIS
      prev_bsdf_pdf   = bs.pdf;
      prev_bsdf_delta = bs.is_delta;

      // Generate next scene ray
      ray = ray_towards_direction(si, to_world(si, bs.wo));
    }

    // TODO RR goes here
    // ...
  } // for (uint depth)

  return S;
}

vec4 Li_debug(in SensorSample sensor_sample, in SamplerState state) {
  return Li_debug(sensor_sample.ray, sensor_sample.wvls, sensor_sample.pdfs, state);
}

vec4 Li(in SensorSample sensor_sample, in SamplerState state) {
  return Li(sensor_sample.ray, sensor_sample.wvls, sensor_sample.pdfs, state);
}

#endif // RENDER_PATH_GLSL_GUARD