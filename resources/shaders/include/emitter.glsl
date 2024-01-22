#ifndef EMITTER_GLSL_GUARD
#define EMITTER_GLSL_GUARD

#include <math.glsl>
#include <ray.glsl>
#include <frame.glsl>
#include <warp.glsl>

// This header requires the following defines to point to SSBOs or shared memory
// to work around glsl's lack of ssbo argument passing
// ...

bool _ray_intersect_sphere(inout Ray ray, in vec3 center, in float r) {
  vec3  o = ray.o - center;
  float a = 1.f;  
  float b = 2.f * dot(o, ray.d);
  float c = sdot(o) - sdot(r);
  float d = b * b - 4.f * a * c;

  float t_near, t_far;

  if (d < 0) {
    return false;
  } else if (d == 0.f) {
    t_near = t_far = -b / 2.f * a;
  } else {
    d = sqrt(d);
    t_near = (-b + d) * 0.5f * a;
    t_far  = (-b - d) * 0.5f * a;
  }

  if (t_near < 0.f)
    t_near = FLT_MAX;
  if (t_far < 0.f)
    t_far = FLT_MAX;
  
  float t = min(t_near, t_far);
  if (t > ray.t || t < 0.f)
    return false;

  ray.t = t;
  return true;
}

void impl_sample_emitter_position_sphere(inout PositionSample ps, in EmitterInfo em, in SurfaceInfo si, in vec2 sample_2d) {
  ps.is_delta = em.sphere_r == 0.f;

  // Sample visible position on sphere
  ps.p = em.center + em.sphere_r * square_to_unif_sphere(sample_2d);
  Ray ray = init_ray(si.p, normalize(ps.p - si.p));
  if (_ray_intersect_sphere(ray, em.center, em.sphere_r)) {
    ps.p = ray_get_position(ray);
    ps.t = ray.t;
  } else {
    ps.t = length(ps.p - si.p);
  }
  ps.n = normalize(ps.p - em.center);
  
  // Store direction to point and keep distance
  ps.d = ray.d;
  
  ps.pdf = em.srfc_area_inv * sdot(ps.t) / abs(dot(ps.d, ps.n));
}

float impl_pdf_emitter_position_sphere(in PositionSample ps, in EmitterInfo em) {
  return em.srfc_area_inv * sdot(ps.t) / abs(dot(ps.d, ps.n));
}

void impl_sample_emitter_position_rect(inout PositionSample ps, in EmitterInfo em, in SurfaceInfo si, in vec2 sample_2d) {
  ps.is_delta = false;
  
  // Sample point on rectangle
  ps.p = (em.trf * vec4(2.f * sample_2d - 1.f, 0, 1)).xyz;
  ps.n = em.rect_n;

  // Store direction to point, normalize, and keep distance
  ps.d = ps.p - si.p;
  ps.t = length(ps.d);
  ps.d /= ps.t;

  // Set pdf to non-zero if we are not approaching from a back-face
  float dp = max(0.f, dot(ps.d, -ps.n));
  ps.pdf = dp > 0.f ? em.srfc_area_inv * sdot(ps.t) / dp : 0.f;
}

float impl_pdf_emitter_position_rect(in PositionSample ps, in EmitterInfo em) {
  float dp = max(0.f, dot(ps.d, -ps.n));
  return dp > 0.f ? em.srfc_area_inv * sdot(ps.t) / dp : 0.f;
}

void impl_sample_emitter_position_point(inout PositionSample ps, in EmitterInfo em, in SurfaceInfo si, in vec2 sample_2d) {
  ps.is_delta = true;

  ps.p = (em.trf * vec4(0, 0, 0, 1)).xyz;
  ps.n = vec3(0, 0, 1); // Indeterminate?
  
  ps.d = ps.p - si.p;
  ps.t = length(ps.d);
  ps.d /= ps.t;
  
  ps.pdf = 1.f;
}

float impl_pdf_emitter_position_point(in PositionSample ps, in EmitterInfo em) {
  return 1.f;
}

void impl_sample_emitter_position_constant(inout PositionSample ps, in EmitterInfo em, in SurfaceInfo si, in vec2 sample_2d) {
  ps.is_delta = true;

  // ...
}

float impl_pdf_emitter_position_constant(in PositionSample ps, in EmitterInfo em) {
  return 0.f;
}

PositionSample sample_emitter_position(in SurfaceInfo si, in uint emitter_i, in vec2 sample_2d) {
  PositionSample ps;
  record_set_emitter(ps.data, emitter_i);

  EmitterInfo em = s_emtr_info[emitter_i];
  if (em.type == EmitterTypeSphere) {
    impl_sample_emitter_position_sphere(ps, em, si, sample_2d);
  } else if (em.type == EmitterTypeRect) {
    impl_sample_emitter_position_rect(ps, em, si, sample_2d);
  } else if (em.type == EmitterTypePoint) {
    impl_sample_emitter_position_point(ps, em, si, sample_2d);
  } else if (em.type == EmitterTypeConstant) {
    impl_sample_emitter_position_constant(ps, em, si, sample_2d);
  }

  return ps;
}

PositionSample sample_emitters_position(in SurfaceInfo si, in vec3 sample_3d) {
  // TODO pick emitter from distribution
  float emitter_sample = sample_3d.z;
  uint  emitter_i      = 0;
  float emitter_pdf    = 1.f;
  
  PositionSample ps = sample_emitter_position(si, emitter_i, sample_3d.xy);
  ps.pdf *= emitter_pdf;
  return ps;
}

float pdf_emitter_position(in PositionSample ps) {
  EmitterInfo em = s_emtr_info[record_get_emitter(ps.data)];
  if (em.type == EmitterTypeSphere) {
    return impl_pdf_emitter_position_sphere(ps, em);
  } else if (em.type == EmitterTypeRect) {
    return impl_pdf_emitter_position_rect(ps, em);
  } else if (em.type == EmitterTypePoint) {
    return impl_pdf_emitter_position_point(ps, em);
  } else {
    return impl_pdf_emitter_position_constant(ps, em);
  }
}

float pdf_emitters_position(in PositionSample ps) {
  // TODO Multiply against emitter pdf from distribution
  return pdf_emitter_position(ps) * 1.f;
}

vec4 eval_emitter_position(in PositionSample ps, in vec4 wvls) {
  // If no emitter is hit, or the normal is not inclined along the ray, return nothing
  vec4 v = vec4(0);
  if ((!ps.is_delta && dot(ps.d, ps.n) >= 0) || !record_is_emitter(ps.data))
    return v;

  // Evaluate emitter throughput
  EmitterInfo em = s_emtr_info[record_get_emitter(ps.data)];
  for (uint i = 0; i < 4; ++i)
    v[i] = texture(b_illm_1f, vec2(wvls[i], em.illuminant_i)).x;
  
  // Attenuate point light
  if (em.type == EmitterTypePoint)
    v /= sdot(ps.t);
    
  return v * em.illuminant_scale;
}

#endif // EMITTER_GLSL_GUARD