#ifndef RENDER_FRESNEL_GLSL_GUARD
#define RENDER_FRESNEL_GLSL_GUARD

float schlick_F0(in float eta) {
  return sdot((1.f - eta) / (1.f + eta));
}

float schlick_fresnel(in float F0, in float F90, in float cos_theta) {
  float c1 = 1.f - cos_theta;
  float c2 = c1 * c1;
  float c5 = c2 * c2 * c1;
  return clamp(F0 + (F90 - F0) * c5, 0.f, 1.f);
}

vec4 schlick_fresnel(in vec4 F0, in vec4 F90, in float cos_theta) {
  float c1 = 1.f - cos_theta;
  float c2 = c1 * c1;
  float c5 = c2 * c2 * c1;
  return clamp(F0 + (F90 - F0) * c5, vec4(0), vec4(1));
}

float schlick_fresnel(in float F0, in float cos_theta) {
  return schlick_fresnel(F0, 1.f, cos_theta);
}

vec4 schlick_fresnel(in vec4 F0, in float cos_theta) {
  return schlick_fresnel(F0, vec4(1), cos_theta);
}

#endif // RENDER_FRESNEL_GLSL_GUARD