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

// TODO hardcoded for now
#define get_dielectric_alpha(brdf) max(1e-3, 0.1 * 0.3)

vec3 to_upper_hemisphere(in vec3 v) {
  return mulsign(v, cos_theta(v));
}

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
  bool  is_reflected = cos_theta(si.wi) * cos_theta(wo) >= 0.f;
  float _eta         = _brdf_eta_dispersive(brdf, wvls.x);

  // Get relative index of refraction along ray
  float     eta = cos_theta(si.wi) > 0 ? _eta : 1.f / _eta;
  float inv_eta = cos_theta(si.wi) > 0 ? 1.f / _eta : _eta;

  // Get the half-vector in the positive hemisphere direction
  vec3 m = to_upper_hemisphere(normalize(si.wi + wo * (is_reflected ? 1.f : eta)));

  // Compute fresnel, angle of transmission
  float cos_theta_t;
  float F = _brdf_dielectric_fresnel(dot(si.wi, m), cos_theta_t, _eta);

  // Evaluate the partial microfacet distribution
  float D_G = eval_microfacet_partial(
    to_upper_hemisphere(si.wi),
    m, 
    to_upper_hemisphere(wo),
    get_dielectric_alpha(brdf)
  );

  if (is_reflected) {
    float weight = 1.f / (4.f * cos_theta(si.wi) * cos_theta(wo));
    return vec4(F * D_G * abs(weight));
  } else {
    float denom = sdot(dot(wo, m) + dot(si.wi, m) * eta) * cos_theta(si.wi) * cos_theta(wo);
    float weight = sdot(inv_eta) * dot(si.wi, m) * dot(wo, m) / abs(denom);
    return vec4((1.f - F) * D_G * abs(weight));
  }
}

float pdf_brdf_dielectric(in BRDF brdf, in Interaction si, in vec3 wo, in vec4 wvls) {
  bool  is_reflected = cos_theta(si.wi) * cos_theta(wo) >= 0.f;
  float _eta         = _brdf_eta_dispersive(brdf, wvls.x);

  // Get relative index of refraction
  float     eta = cos_theta(si.wi) > 0 ? _eta : 1.f / _eta;
  float inv_eta = cos_theta(si.wi) > 0 ? 1.f / _eta : _eta;

  // Get the half-vector in the positive hemisphere direction
  vec3 m = to_upper_hemisphere(normalize(si.wi + mix(eta, 1.f, is_reflected) * wo));

  // Compute fresnel
  float cos_theta_t;
  float F = _brdf_dielectric_fresnel(dot(si.wi, m), cos_theta_t, _eta);

  float pdf = pdf_microfacet(
    to_upper_hemisphere(si.wi), 
    m, 
    get_dielectric_alpha(brdf)
  );

  if (is_reflected) {
    float weight = 1.f / (4.f * dot(wo, m));
    return pdf * F * abs(weight);
  } else {
    float weight = sdot(inv_eta) * dot(wo, m) 
                 / sdot(dot(wo, m) + dot(si.wi, m) * eta);
    return pdf * (1.f - F) * abs(weight);
  }
}

BRDFSample sample_brdf_dielectric(in BRDF brdf, in vec3 sample_3d, in Interaction si, in vec4 wvls) {
  // Sample a microfacet normal to reflect/refract on
  MicrofacetSample ms = sample_microfacet(
    to_upper_hemisphere(si.wi),
    get_dielectric_alpha(brdf), 
    sample_3d.yz
  );

  // Compute fresnel and angle of transmission
  float    _eta = _brdf_eta_dispersive(brdf, wvls.x);
  float cos_theta_t;
  float F = _brdf_dielectric_fresnel(dot(si.wi, ms.n), cos_theta_t, _eta);

  // Get relative index of refraction
  float     eta = cos_theta(si.wi) > 0 ? _eta : 1.f / _eta;
  float inv_eta = cos_theta(si.wi) > 0 ? 1.f / _eta : _eta;

  // Return object
  BRDFSample bs;
  bs.is_delta = false;
  
  Frame local_fr = get_frame(ms.n);
  vec3  local_wi = to_local(local_fr, si.wi);

  // Pick reflection/refraction lobe
  if (sample_3d.x < F) {
    // Reflect on microfacet normal
    vec3 wo = local_reflect(local_wi);

    bs.is_spectral = false;
    bs.wo          = to_world(local_fr, wo);

    float weight = 1.f / (4.f * dot(bs.wo, ms.n));
    bs.pdf = F * ms.pdf * abs(weight);
  } else {
    // Refract on microfacet normal
    vec3 wo = local_refract(local_wi, cos_theta_t, inv_eta);

    bs.is_spectral = get_dielectric_is_dispersive(brdf);
    bs.wo          = to_world(local_fr, wo);

    float weight = sdot(inv_eta) * dot(bs.wo, ms.n)
                 / sdot(dot(bs.wo, ms.n) + dot(si.wi, ms.n) * eta);
    bs.pdf = (1.f - F) * ms.pdf * abs(weight);
  }

  return bs;
}

#endif // BRDF_DIELECTRIC_GLSL_GUARD