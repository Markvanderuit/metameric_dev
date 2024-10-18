#ifndef BRDF_PRINCIPLED_GLSL_GUARD
#define BRDF_PRINCIPLED_GLSL_GUARD

#include <render/warp.glsl>
#include <render/record.glsl>

float ggx() {
  // ...
}

float smith_g_ggx(float n_dot_v, float alpha_2) {
  float n_dot_v_2 = n_dot_v * n_dot_v;
  return safe_rcp(abs(n_dot_v) + safe_sqrt(alpha_2 + n_dot_v_2 - alpha_2 * n_dot_v_2));
}

vec4 schlick_fresnel(vec4 f0, float cos_theta_d) {
  float f1 = 1.f - cos_theta_d;
  float f2 = f1 * f1;
  float f5 = f2 * f2 * f1;
  return f0 + (vec4(1) - f0) * f5;
}

float G() {
  // ...
}

float D() {
  // ...
}

float F() {
  // ...
}

void init_brdf_principled(inout BRDFInfo brdf, in SurfaceInfo si, vec4 wvls) {
  // ...
}

BRDFSample sample_brdf_principled(in BRDFInfo brdf, in vec2 sample_2d, in SurfaceInfo si) {
  BRDFSample bs;

  // ...

  return bs;
}

vec4 eval_brdf_principled(in BRDFInfo brdf, in SurfaceInfo si, in vec3 wo) {

// ---
  return vec4(0);
}

float pdf_brdf_principled(in BRDFInfo brdf, in SurfaceInfo si, in vec3 wo) {

// ---
  return 0.f;
}

#endif // BRDF_PRINCIPLED_GLSL_GUARD
