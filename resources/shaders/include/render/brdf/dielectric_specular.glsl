#ifndef BRDF_dielectric_GLSL_GUARD
#define BRDF_dielectric_GLSL_GUARD

#include <render/record.glsl>

// Accessors to BRDFInfo data
#define get_dielectric_r(brdf)             brdf.r
#define get_dielectric_cauchy_b(brdf)      brdf.data.x
#define get_dielectric_cauchy_c(brdf)      brdf.data.y
#define get_dielectric_eta(brdf)           brdf.data.x
#define get_dielectric_is_dispersive(brdf) (brdf.data.y != 0)
#define get_dielectric_absorption(brdf)    brdf.data.z

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

float _brdf_eta_dispersive(in BRDFInfo brdf, in float x) {
  if (get_dielectric_is_dispersive(brdf)) {
    // Cauchy's equation
    return get_dielectric_cauchy_b(brdf) + get_dielectric_cauchy_c(brdf) / sdot(sample_to_wavelength(x));
  } else {
    return get_dielectric_eta(brdf);
  }
}

void init_brdf_dielectric(in ObjectInfo object, inout BRDFInfo brdf, in SurfaceInfo si, vec4 wvls, in vec2 sample_2d) {
  get_dielectric_r(brdf)          = texture_reflectance(si, wvls, sample_2d);
  get_dielectric_absorption(brdf) = object.absorption;

  float eta_min = object.eta_minmax.x, eta_max = object.eta_minmax.y;

  if (eta_min == eta_max || eta_min > eta_max) {
    // Effectively disables _brdf_eta_dispersive(...) in case configuration isn't spectral
    get_dielectric_eta(brdf) = eta_min;
    get_dielectric_cauchy_c(brdf) = 0;
  } else {
    // Compute cauchy coefficients b and c
    float lambda_min_2 = wavelength_min * wavelength_min, 
          lambda_max_2 = wavelength_max * wavelength_max;
    get_dielectric_cauchy_b(brdf) = (lambda_min_2 * eta_max - lambda_max_2 * eta_min) 
                                  / (lambda_min_2 - lambda_max_2);
    get_dielectric_cauchy_c(brdf) = lambda_min_2 * (eta_max - get_dielectric_cauchy_b(brdf));
  }
}

BRDFSample sample_brdf_dielectric(in BRDFInfo brdf, in vec3 sample_3d, in SurfaceInfo si) {
  // Compute eta for hero wavelength only, the rest gets killed later on
  // Compute fresnel, angle of transmission
  float eta = _brdf_eta_dispersive(brdf, brdf.wvls.x);
  float cos_theta_t;
  float F = _brdf_dielectric_fresnel(cos_theta(si.wi), cos_theta_t, eta);

  // Pick reflection/refraction
  bool is_transmitted = sample_3d.x > F;

  BRDFSample bs;

  bs.is_delta    = true;
  bs.is_spectral = is_transmitted && get_dielectric_is_dispersive(brdf);
  bs.wo          = is_transmitted ? local_refract(si.wi, cos_theta_t, eta) : local_reflect(si.wi);
  bs.pdf         = is_transmitted ? 1.f - F : F;

  return bs;
}

vec4 eval_brdf_dielectric(in BRDFInfo brdf, in SurfaceInfo si, in vec3 wo) {
  // Compute eta for hero wavelength only, the rest gets killed later on
  // Compute fresnel, angle of transmission
  float eta = _brdf_eta_dispersive(brdf, brdf.wvls.x);
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

float pdf_brdf_dielectric(in BRDFInfo brdf, in SurfaceInfo si, in vec3 wo) {
  return 0.f;
}

#endif // BRDF_dielectric_GLSL_GUARD