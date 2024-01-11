#ifndef EMITTER_GLSL_GUARD
#define EMITTER_GLSL_GUARD

#include <math.glsl>
#include <ray.glsl>

// This header requires the following defines to point to SSBOs or shared memory
// to work around glsl's lack of ssbo argument passing

vec3 square_to_unif_cone(float cos_cutoff, in vec2 sample_2d) {
  float cos_theta = (1.f - sample_2d.x) + sample_2d.x * cos_cutoff;
  float sin_theta = sqrt(max(0.0001f, 1.f - cos_theta * cos_theta));

  float sin_phi = sin(2.f * M_PI * sample_2d.y);
  float cos_phi = cos(2.f * M_PI * sample_2d.y);

  return vec3(cos_phi * sin_theta, sin_phi * sin_theta, cos_theta);
}

/* float square_to_unif_cone_pdf(float cos_cutoff, in vec2 sample_2d) {
  return .5f * M_PI_INV / (1.f - cos_cutoff);
} */

vec3 square_to_unif_sphere(in vec2 sample_2d) {
  float z = 1.f - 2.f * sample_2d.y;
  float r = sqrt(max(0.00001f, 1.f - z * z));

  float sin_phi = sin(2.f * M_PI * sample_2d.x);
  float cos_phi = cos(2.f * M_PI * sample_2d.x);
  
  return vec3(r * cos_phi, r * sin_phi, z);
}

struct EmitterSample {
  vec4  value;
  vec3  p;
  float pdf;
};

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

EmitterSample sample_emitter(in EmitterInfo em, SurfaceInfo si, in vec4 wvls, in vec2 sample_2d) {
  EmitterSample es;
  
  // TODO refer to texture outside include
  for (uint i = 0; i < 4; ++i)
    es.value[i] = texture(b_illm_1f, vec2(wvls[i], em.illuminant_i)).x;
  es.value *= em.illuminant_scale;

  if (em.type == EmitterTypeSphere) {
    // Naive; pick a point on sphere
    float r = (em.trf * vec4(1, 0, 0, 0)).x;
    es.p   = (em.trf * vec4(square_to_unif_sphere(sample_2d), 1)).xyz;
    es.pdf = 1.f / (4.f * M_PI * r * r);

    /* vec3  p_surface        = (em.trf_inv * vec4(si.p, 1)).xyz;
    vec3  d_to_center      = -p_surface;
    float dist_to_center_2 = sdot(d_to_center);
    float inv_dist_to_cent = 1.f / sqrt(invRefDist);
    float sin_alpha        = inv_dist_to_cent;

    if (sin_alpha < 1.f) {
      float cos_alpha = sqrt(max(0.0001f, 1.f - sin_alpha * sin_alpha));

    } else {

    } */

  } else if (em.type == EmitterTypeRect) {
    es.p   = (em.trf * vec4(2.f * sample_2d - 1.f, 0, 1)).xyz;
    es.pdf = 1.f / hprod((em.trf * vec4(2, 2, 0, 1)).xyz);
  } else if (em.type == EmitterTypePoint) {
    es.p   = (em.trf * vec4(0, 0, 0, 1)).xyz;
    es.pdf = 1.f;
  } else if (em.type == EmitterTypeConstant) {
    es.pdf = 0.f;
  }

  return es;
}

vec4 eval_emitter(in EmitterInfo em, in vec4 wvls, in vec3 p) {
  // TODO refer to texture outside include
  vec4 v;
  for (uint i = 0; i < 4; ++i)
    v[i] = texture(b_illm_1f, vec2(wvls[i], em.illuminant_i)).x;
  return v * em.illuminant_scale;
}

float pdf_emitter(in EmitterInfo em, in vec3 p) {
  if (em.type == EmitterTypeSphere) {
    float r = (em.trf * vec4(1, 0, 0, 0)).x;
    return 1.f / (4.f * M_PI * r * r);
  } else if (em.type == EmitterTypeRect) {
    return 1.f / hprod((em.trf * vec4(2, 2, 0, 1)).xyz);
  } else if (em.type == EmitterTypePoint) {
    return 1.f;
  } else {
    return 0.f;
  }
}


#endif // EMITTER_GLSL_GUARD