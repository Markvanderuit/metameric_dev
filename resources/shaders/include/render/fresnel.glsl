#ifndef RENDER_FRESNEL_GLSL_GUARD
#define RENDER_FRESNEL_GLSL_GUARD

float schlick_F0(in float eta_a, in float eta_b) {
  return sdot((eta_a - eta_b) / (eta_a + eta_b));
}

float schlick_fresnel(in float F0, in float F90, in float cos_theta_i) {
  float c1 = 1.f - cos_theta_i;
  float c2 = c1 * c1;
  float c5 = c2 * c2 * c1;
  return clamp(F0 + (F90 - F0) * c5, 0.f, 1.f);
}

vec4 schlick_fresnel(in vec4 F0, in vec4 F90, in float cos_theta_i) {
  float c1 = 1.f - cos_theta_i;
  float c2 = c1 * c1;
  float c5 = c2 * c2 * c1;
  return clamp(F0 + (F90 - F0) * c5, vec4(0), vec4(1));
}

float schlick_fresnel(in float F0, in float cos_theta_i) {
  return schlick_fresnel(F0, 1.f, cos_theta_i);
}

vec4 schlick_fresnel(in vec4 F0, in float cos_theta_i) {
  return schlick_fresnel(F0, vec4(1), cos_theta_i);
}

float schlick_fresnel(in float F0, in float cos_theta_i, inout float cos_theta_t, in float eta) {
  float scale         = cos_theta_i > 0 ? 1.f / eta : eta;
  float cos_theta_t_2 = 1.f - (1.f - sdot(cos_theta_i)) * sdot(scale);
  
  // Total internal reflection check before computing angle of transmission
  if (cos_theta_t_2 <= 0.f) {
    cos_theta_t = 0.f;
    return 1.f;
  }
  cos_theta_t = mulsign(sqrt(cos_theta_t_2), -cos_theta_i);

  return schlick_fresnel(F0, abs(cos_theta_i));
}

vec4 schlick_fresnel(in vec4 F0, in float cos_theta_i, inout float cos_theta_t, in float eta) {
  float scale         = cos_theta_i > 0 ? 1.f / eta : eta;
  float cos_theta_t_2 = 1.f - (1.f - sdot(cos_theta_i)) * sdot(scale);
  
  // Total internal reflection check before computing angle of transmission
  if (cos_theta_t_2 <= 0.f) {
    cos_theta_t = 0.f;
    return vec4(1);
  }
  cos_theta_t = mulsign(sqrt(cos_theta_t_2), -cos_theta_i);

  return schlick_fresnel(F0, abs(cos_theta_i));  
}

#endif // RENDER_FRESNEL_GLSL_GUARD