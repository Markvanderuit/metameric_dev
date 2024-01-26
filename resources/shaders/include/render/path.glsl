#ifndef RENDER_PATH_GLSL_GUARD
#define RENDER_PATH_GLSL_GUARD

#include <sampler/uniform.glsl>
#include <render/ray.glsl>
#include <render/scene.glsl>
#include <render/sensor.glsl>

// Helper struct to cache path vertex information
struct PathVertex {
  // World position
  vec3 p;

  // Record storing object/emitter/primitive id, see record.glsl
  uint data;
};

// Helper struct to cache path information,
// but without surface reflectances, which are ignored
struct IncompletePath {
  // Sampled path wavelengths
  vec4 wvls;

  // Energy over probability density, 
  // not decreased by surface reflectances
  vec4 L; 

  // Total length of path before termination
  uint path_depth;

  // Path vertex information, up to path_depth
  PathVertex data[max_depth];
};

// Helper struct to cache full path information
struct FullPath {
  // Sampled path wavelengths
  vec4 wvls;

  // Energy over probability density
  vec4 L; 

  // Total length of path before termination
  uint path_depth;

  // Path vertex information, up to path_depth
  PathVertex data[max_depth];
};

vec4 Li(in Ray ray, in vec4 wvls, in SamplerState state) {
  // Path throughput information; we track 4 wavelengths simultaneously
  vec4  s               = vec4(0.f);
  vec4  beta            = vec4(1.f);
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

    // If an emissive object is hit directly, add contribution to path
    if (is_emitter(si)) {
      PositionSample ps       = get_position_sample(si);
      float          emtr_pdf = prev_bsdf_delta ? 0.f : pdf_emitters(ps);

      // No division by sample density, as this is incorporated in path throughput
      s += beta                                // throughput 
         * eval_emitter(ps, wvls)              // emitted value
         * mis_power(prev_bsdf_pdf, emtr_pdf); // mis weight
    }

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
          s += beta                         // throughput
             * eval_brdf(brdf, si, wo)      // brdf value
             * cos_theta(wo)                // cosine attenuation
             * eval_emitter(ps, wvls)       // emitted value
             * mis_power(ps.pdf, bsdf_pdf)  // mis weight
             / ps.pdf;                      // sample density
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
            * cos_theta(bs.wo)  // Cosine attenuation
            / bs.pdf;           // sample density
      
      // Early exit on zero throughput
      if (all(iszero(beta)))
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

  return s;
}

vec4 Li(in SensorSample sensor_sample, in SamplerState state) {
  return Li(sensor_sample.ray, sensor_sample.wvls, state) / sensor_sample.pdfs;
}

#endif // RENDER_PATH_GLSL_GUARD