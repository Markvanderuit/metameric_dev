#ifndef RENDER_INTEGRATION_GLSL_GUARD
#define RENDER_INTEGRATION_GLSL_GUARD

#include <render/ray.glsl>
#include <render/scene.glsl>
#include <render/sensor.glsl>
#include <sampler/uniform.glsl>

float mis_balance(in float pdf_a, in float pdf_b) {
  return pdf_a / (pdf_a + pdf_b);
}

float mis_power(in float pdf_a, in float pdf_b) {
  pdf_a *= pdf_a;
  pdf_b *= pdf_b;
  return pdf_a / (pdf_a + pdf_b);
}

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
      PositionSample ps = get_position_sample(si);
      vec4           L  = eval_emitter(ps, wvls);
      float emtr_pdf    = prev_bsdf_delta ? 0.f : pdf_emitters(ps);
      
      s += beta * L * mis_power(prev_bsdf_pdf, emtr_pdf);
    }

    // Sample BRDF at position
    BRDFInfo brdf = get_brdf(si, wvls);
    
    // Direct illumination sampling;
    {
      // Generate position sample on emitter
      PositionSample ps       = sample_emitters(si, next_3d(state));
      float          bsdf_pdf = pdf_brdf(brdf, si, ps.d);
      
      // If the sample position has potential throughput, 
      // evaluate a ray towards the position  
      if (ps.pdf != 0.f && bsdf_pdf != 0.f && dot(ps.d, si.sh.n) > 0.f) {
        Ray ray = ray_towards_point(si, ps.p);
        if (!scene_intersect_any(ray)) {
          // On a success, add contribution to path
          vec4 L = eval_emitter(ps, wvls) / ps.pdf;
          vec4 f = eval_brdf(brdf, si, ps.d);
          s += beta * f * L * mis_power(ps.pdf, prev_bsdf_pdf * bsdf_pdf);
        }
      }
    }

    // BRDF sampling; 
    {
      BRDFSample bs = sample_brdf(brdf, next_2d(state), si);
      if (bs.pdf == 0.f)
        break;
      
      // Update throughput
      beta *= bs.f;
      if (all(iszero(beta)))
        break;

      // Store previous BRDF information for MIS
      prev_bsdf_pdf   = bs.pdf;
      prev_bsdf_delta = bs.is_delta;

      // Generate next scene ray
      ray = ray_towards_direction(si, bs.wo);
    }

    // TODO RR goes here
    // ...
  } // for (uint depth)

  return s;
}

vec4 Li(in SensorSample sensor_sample, in SamplerState state) {
  return Li(sensor_sample.ray, sensor_sample.wvls, state) / sensor_sample.pdfs;
}

#endif // RENDER_INTEGRATION_GLSL_GUARD