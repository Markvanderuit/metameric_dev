#ifndef EMITTER_GLSL_GUARD
#define EMITTER_GLSL_GUARD

#include <math.glsl>
#include <ray.glsl>
#include <frame.glsl>
#include <warp.glsl>

// This header requires the following defines to point to SSBOs or shared memory
// to work around glsl's lack of ssbo argument passing



/* vec3 sample_position_unit_rect(in vec2 sample_2d) {
  return vec3(sample_2d * 2.f - 1.f, 0);
} */

/* vec3 sample_position_unit_sphere(in vec2 sample_2d) {
  return vec3(0);
} */

/* float pdf_position_unit_sphere(in vec3 p) {
  return 1.f;
} */

/* float pdf_position_unit_rect(in vec3 p) {
  return 1.f;
} */

bool _ray_intersect_unit_sphere(inout Ray ray) {
  float b = dot(ray.o, ray.d) * 2.f;
  float c = sdot(ray.o) - 1.f;

  float discrim = b * b - 4.f * c;
  if (discrim < 0)
    return false;
  
  float t_near = -.5f * (b + sqrt(discrim) * (b >= 0 ? 1.f : -1.f));
  float t_far  = c / t_near;

  if (t_near > t_far)
    swap(t_near, t_far);

  if (t_far < 0.f || t_near > ray.t)
    return false;

  if (t_far > ray.t && t_near < 0.f)
    return false;

  ray.t = t_near;
  return true;
}

struct EmitterSample {
  vec4  L;
  vec3  p;
  vec3  n;
  float pdf;
};

void _sample_emitter_sphere(inout EmitterSample es, in EmitterInfo em, in SurfaceInfo si, in vec4 wvls, in vec2 sample_2d) {
  // Slightly better method; sample aligned hemisphere

  // vec3 o = (em.trf_inv * vec4(si.p, 1)).xyz;
  // vec3 p = square_to_unif_hemisphere(sample_2d);

  // // TODO debug that the frame is actually correct
  // Frame frm = get_frame(normalize(o));
  // p = frame_to_world(frm, p);

  // // Run intersect to find closest matching point
  // Ray ray = init_ray(o, normalize(p - o));
  // if (_ray_intersect_unit_sphere(ray)) { 
  //   p = ray.o + ray.t * ray.d;
  // }

  // float r = (em.trf * vec4(1, 0, 0, 0)).x;
  // es.p    = (em.trf * vec4(p, 1)).xyz;
  // es.pdf  = 1.f / (4.f * M_PI * r * r); // TODO precompute in em.surface_area_inv

  // Bad method; pick a point on sphere, then find nearest intersection to point
  
  vec3 o = (em.trf_inv * vec4(si.p, 1)).xyz;
  vec3 p = square_to_unif_sphere(sample_2d);

  Ray ray = init_ray(o, normalize(p - o));
  if (_ray_intersect_unit_sphere(ray)) { 
    p = ray.o + ray.t * ray.d;
  }
  
  vec3  d  = p - o;
  float t2 = sdot(p - o);
  d *= inversesqrt(t2);

  float r = (em.trf * vec4(1, 0, 0, 0)).x;
  es.p    = (em.trf * vec4(p, 1)).xyz;
  // es.pdf  = 1.f / (4.f * M_PI * r * r);
  es.pdf  = t2  / abs(dot(d, p))
          * 1.f / (4.f * M_PI * r * r);
          
  // Worse method; pick a point on sphere

  // float r = (em.trf * vec4(1, 0, 0, 0)).x;
  // es.p    = (em.trf * vec4(square_to_unif_sphere(sample_2d), 1)).xyz;
  // es.pdf  = 1.f / (4.f * M_PI * r * r);
}

float _pdf_emitter_sphere(in EmitterInfo em, in SurfaceInfo si, in vec3 p) {
  float r = (em.trf * vec4(1, 0, 0, 0)).x;
  vec3  o = (em.trf_inv * vec4(si.p, 1)).xyz; 
  
  vec3  p_ = (em.trf_inv * vec4(p, 1)).xyz;
  vec3  d  =  p_ - o;
  float t2 = sdot(p - o);
  d *= inversesqrt(t2);
  
  // return 1.f / (4.f * M_PI * r * r);
  return t2 / abs(dot(d, p))
       * 1.f / (4.f * M_PI * r * r);
}

void _sample_emitter_sphere_solid_angle(inout EmitterSample es, in EmitterInfo em, in SurfaceInfo si, in vec4 wvls, in vec2 sample_2d) {
  // Copied from mitsuba 3; direct copy of Sphere::sample_direction(...) -> DirectionSample3f
  
  vec3 dc_v  = em.center - si.p;
  float dc_2 = sdot(dc_v);

  if (dc_2 > sdot(em.r * (1.f - M_RAY_EPS))) {
    float inv_dc          = inversesqrt(dc_2);
    float sin_theta_max   = em.r * inv_dc;
    float sin_theta_max_2 = sdot(sin_theta_max);
    float inv_sin_theta_max = 1.f / sin_theta_max;
    float cos_theta_max     = sqrt(max(0.f, 1.f - sin_theta_max_2));
    
    float sin_theta_2 = (sin_theta_max_2 > 0.00068523f)
                      ? 1.f - sdot(fma(cos_theta_max - 1.f, sample_2d.x, 1.f))
                      : sin_theta_max_2 * sample_2d.x;
    float cos_theta = sqrt(max(0.f, 1.f - sin_theta_2));

    float cos_alpha = sin_theta_2 * inv_sin_theta_max 
                    + cos_theta * sqrt(max(0.f, fma(-sin_theta_2, sdot(inv_sin_theta_max), 1.f)));
    float sin_alpha = sqrt(max(0.00001f, fma(-cos_alpha, cos_alpha, 1.f)));

    float cos_phi = cos(sample_2d.y * (2.f * M_PI));
    float sin_phi = sin(sample_2d.y * (2.f * M_PI));

    vec3 d = frame_to_world(get_frame(dc_v * -inv_dc), vec3(cos_phi * sin_alpha,
                                                                       sin_phi * sin_alpha,
                                                                       cos_alpha));
    es.n   = d;  
    es.p   = fma(d, vec3(em.r), em.center);
    es.pdf = square_to_unif_cone_pdf(vec2(0), cos_theta_max);
  } else {
    vec3 d = square_to_unif_sphere(sample_2d);
    es.p   = fma(d, vec3(em.r), em.center);
    es.n   = d;

    vec3  si_d = es.p - si.p;
    float t2 = sdot(es.p - si.p);
    si_d *= inversesqrt(t2);
    
    es.pdf = (t2 / abs(dot(si_d, es.n))) * em.srfc_area_inv;
  }
}

float _pdf_emitter_sphere_solid_angle(in EmitterInfo em, in SurfaceInfo si, in vec3 p) {
  // Copied from mitsuba 3; direct copy of Sphere::pdf_direction(...) -> f32
  vec3 d = p - si.p;
  float t2 = sdot(d);
  d *= inversesqrt(t2);

  // TODO precomp in surface object
  vec3 n = normalize(p - em.center.xyz);
  
  float sin_alpha = em.r / length(em.center.xyz - si.p);
  float cos_alpha = sqrt(max(0.f, 1.f - sin_alpha * sin_alpha));

  return sin_alpha < (1.f - M_EPS) ? square_to_unif_cone_pdf(vec2(0), cos_alpha)
                                   : (t2 / abs(dot(d, n))) * em.srfc_area_inv;

  ////

  // float r = (em.trf * vec4(1, 0, 0, 0)).x;
  // vec3  o = (em.trf_inv * vec4(si.p, 1)).xyz; 
  // vec3  p  = (em.trf_inv * vec4(p_, 1)).xyz;

  // // Direction and distance towards sphere sample p
  // vec3  d  = p - o;
  // float t2 = sdot(d);
  // d *= inversesqrt(t2);

  // vec3 n = normalize(p);
  
  // float sin_alpha = 1.f / length(o);
  // float cos_alpha = sqrt(max(0.f, 1.f - sin_alpha * sin_alpha));
  
  // return sin_alpha < (1.f - M_EPS) ? square_to_unif_cone_pdf(vec2(0), cos_alpha)
  //                                  : (t2 / abs(dot(d, n))) * (1.f / (4.f * M_PI * r * r));
}

void _sample_emitter_rect(inout EmitterSample es, in EmitterInfo em, in SurfaceInfo si, in vec4 wvls, in vec2 sample_2d) {
  es.p   = (em.trf * vec4(2.f * sample_2d - 1.f, 0, 1)).xyz;
  es.pdf = 1.f / hprod((em.trf * vec4(2, 2, 0, 1)).xyz);
}

float _pdf_emitter_rect(in EmitterInfo em, in SurfaceInfo si, in vec3 p) {
    // float t2 = sdot(si.p - p);

    return 1.f / hprod((em.trf * vec4(2, 2, 0, 1)).xyz);
}

void _sample_emitter_point(inout EmitterSample es, in EmitterInfo em, in SurfaceInfo si, in vec4 wvls, in vec2 sample_2d) {
  es.p   = (em.trf * vec4(0, 0, 0, 1)).xyz;
  es.pdf = 1.f;
}

float _pdf_emitter_point(in EmitterInfo em, in SurfaceInfo si, in vec3 p) {
  return 1.f;
}

void _sample_emitter_constant(inout EmitterSample es, in EmitterInfo em, in SurfaceInfo si, in vec4 wvls, in vec2 sample_2d) {
  es.p   = vec3(0);
  es.pdf = 0.f;
}

float _pdf_emitter_constant(in EmitterInfo em, in SurfaceInfo si, in vec3 p) {
  return 0.f;
}

vec4 eval_emitter(in EmitterInfo em, in vec4 wvls, in vec3 p) {
  // TODO refer to texture outside include
  vec4 v;
  for (uint i = 0; i < 4; ++i)
    v[i] = texture(b_illm_1f, vec2(wvls[i], em.illuminant_i)).x;
  return v * em.illuminant_scale;
}

EmitterSample sample_emitter(in EmitterInfo em, in SurfaceInfo si, in vec4 wvls, in vec2 sample_2d) {
  EmitterSample es;
  es.L = eval_emitter(em, wvls, es.p);
  
  // Forward to specific type sample
  if (em.type == EmitterTypeSphere) {
    // _sample_emitter_sphere(es, em, si, wvls, sample_2d);
    _sample_emitter_sphere_solid_angle(es, em, si, wvls, sample_2d);
  } else if (em.type == EmitterTypeRect) {
    _sample_emitter_rect(es, em, si, wvls, sample_2d);
  } else if (em.type == EmitterTypePoint) {
    _sample_emitter_point(es, em, si, wvls, sample_2d);
  } else if (em.type == EmitterTypeConstant) {
    _sample_emitter_constant(es, em, si, wvls, sample_2d);
  }

  return es;
}

float pdf_emitter(in EmitterInfo em, in SurfaceInfo si, in vec3 p) {
  if (em.type == EmitterTypeSphere) {
    // return _pdf_emitter_sphere(em, si, p);
    return _pdf_emitter_sphere_solid_angle(em, si, p);
  } else if (em.type == EmitterTypeRect) {
    return _pdf_emitter_rect(em, si, p);
  } else if (em.type == EmitterTypePoint) {
    return _pdf_emitter_point(em, si, p);
  } else {
    return _pdf_emitter_constant(em, si, p);
  }
}

#endif // EMITTER_GLSL_GUARD