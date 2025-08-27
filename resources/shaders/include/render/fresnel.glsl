#ifndef RENDER_FRESNEL_GLSL_GUARD
#define RENDER_FRESNEL_GLSL_GUARD

float cos_theta_refract(in float cos_theta_i, in float eta) {
  float scale         = cos_theta_i > 0 ? 1.f / eta : eta;
  float cos_theta_t_2 = 1.f - (1.f - sdot(cos_theta_i)) * sdot(scale);
  float cos_theta_t   = sqrt(cos_theta_t_2);
  return cos_theta_i > 0 ? -cos_theta_t : cos_theta_t;
}

// Source, Mitsuba 0.5, util.cpp, line 651
float dielectric_fresnel(in float cos_theta_i, inout float cos_theta_t, in float eta) {
  float scale = (cos_theta_i > 0) ? 1.f / eta : eta;
  float cos_theta_t_2 = 1.f - (1.f - sdot(cos_theta_i)) * sdot(scale);
  
  // Total internal reflection check:
  if (cos_theta_t_2 <= 0.f) {
    cos_theta_t = 0.f;
    return 1.f;
  }

  float cos_theta_a = abs(cos_theta_i);
        cos_theta_t = sqrt(cos_theta_t_2);
  float rs = (cos_theta_a - eta * cos_theta_t)
           / (cos_theta_a + eta * cos_theta_t);
  float rp = (eta * cos_theta_a - cos_theta_t)
           / (eta * cos_theta_a + cos_theta_t);

  cos_theta_t = (cos_theta_i > 0) ? -cos_theta_t : cos_theta_t;
  
  // Unpolarized
  return 0.5f * (rs * rs + rp * rp);
}

vec4 schlick_fresnel(in vec4 f0, in vec4 f90, in float cos_theta) {
  float c1 = 1.f - cos_theta;
  float c2 = c1 * c1;
  float c5 = c2 * c2 * c1;
  return clamp(f0 + (f90 - f0) * c5, vec4(0), vec4(1));
}

vec4 schlick_fresnel(in vec4 f0, in float cos_theta) {
  return schlick_fresnel(f0, vec4(1), cos_theta);
}

#endif // RENDER_FRESNEL_GLSL_GUARD