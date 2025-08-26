#ifndef BRDF_DIELECTRIC_GLSL_GUARD
#define BRDF_DIELECTRIC_GLSL_GUARD

#include <render/record.glsl>

// Accessors to BRDF data
#define get_dielectric_r(brdf)             brdf.r
#define get_dielectric_cauchy_b(brdf)      brdf.data.x
#define get_dielectric_cauchy_c(brdf)      brdf.data.y
#define get_dielectric_eta(brdf)           brdf.data.x
#define get_dielectric_absorption(brdf)    brdf.data.z
#define get_dielectric_is_dispersive(brdf) (brdf.data.y != 0)

// Source, Mitsuba 0.5, util.cpp, line 651
float _brdf_dielectric_fresnel(in float cos_theta_i, inout float cos_theta_t, in float eta) {
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

float _brdf_eta_dispersive(in BRDF brdf, in float x) {
  if (get_dielectric_is_dispersive(brdf)) {
    // Cauchy's equation
    return get_dielectric_cauchy_b(brdf) + get_dielectric_cauchy_c(brdf) / sdot(sample_to_wavelength(x));
  } else {
    return get_dielectric_eta(brdf);
  }
}

vec4 eval_brdf_dielectric(in BRDF brdf, in Interaction si, in vec3 wo, in vec4 wvls) {
  // Compute eta for hero wavelength only, the rest gets killed later on
  // Compute fresnel, angle of transmission
  float eta = _brdf_eta_dispersive(brdf, wvls.x);
  float cos_theta_t;
  float F = _brdf_dielectric_fresnel(cos_theta(si.wi), cos_theta_t, eta);
  
  // Select a lobe based on opposing directions
  if (cos_theta(si.wi) * cos_theta(wo) >= 0) {
    // Scatter, reflect
    return vec4(F / abs(cos_theta(wo)));
  } else {
    // Transmit, refract, add beer
    vec4 r = wo.z < 0 // exiting
           ? vec4(1)
           : exp(-get_dielectric_absorption(brdf) * (vec4(1) - get_dielectric_r(brdf)) * si.t);
    float scaling = sdot(cos_theta_t < 0.f ? 1.f / eta : eta);
    return r * scaling * (1.f - F) / abs(cos_theta(wo));
  }
}

float pdf_brdf_dielectric(in BRDF brdf, in Interaction si, in vec3 wo) {
  return 0.f;
}

BRDFSample sample_brdf_dielectric(in BRDF brdf, in vec3 sample_3d, in Interaction si, in vec4 wvls) {
  float _eta = _brdf_eta_dispersive(brdf, wvls.x);

  // Compute fresnel, angle of transmission
  float cos_theta_t;
  float F = _brdf_dielectric_fresnel(cos_theta(si.wi), cos_theta_t, _eta);

  // Return object
  BRDFSample bs;
  bs.is_delta    = true;
  
  if (sample_3d.z <= F) {
    // Reflect
    bs.is_spectral = false;
    bs.wo          = local_reflect(si.wi);
    bs.pdf         = F;
  } else {
    // Transmit
    bs.is_spectral = get_dielectric_is_dispersive(brdf);
    bs.wo          = local_refract(si.wi, cos_theta_t, cos_theta_t > 0 ? _eta : 1.f / _eta;);
    bs.pdf         = 1.f - F;
  }
  
  return bs;
}

#endif // BRDF_DIELECTRIC_GLSL_GUARD