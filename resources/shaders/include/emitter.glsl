#ifndef EMITTER_GLSL_GUARD
#define EMITTER_GLSL_GUARD

#include <ray.glsl>

// This header requires the following defines to point to SSBOs or shared memory
// to work around glsl's lack of ssbo argument passing

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

EmitterSample sample_emitter(in EmitterInfo em, in vec2 sample_2d) {
  EmitterSample ps;
  return ps;
}

vec4 eval_emitter(in EmitterInfo em, in vec4 wvls, in vec3 p) {
  // TODO refer to texture outside include
  vec4 v;
  for (uint i = 0; i < 4; ++i)
    v[i] = texture(b_illm_1f, vec2(wvls[i], em.illuminant_i)).x;
  return v * em.illuminant_scale;
}

float pdf_position(in EmitterInfo em, in vec3 p) {
  return 0.f;
}


#endif // EMITTER_GLSL_GUARD