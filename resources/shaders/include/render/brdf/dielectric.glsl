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
#define get_dielectric_alpha(brdf) 0.01

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

  // Get relative index of refraction
  float     eta = cos_theta(si.wi) > 0 ? _eta : 1.f / _eta;
  float inv_eta = cos_theta(si.wi) > 0 ? 1.f / _eta : _eta;

  // Get the half-vector in the positive hemisphere direction
  vec3 m = normalize(si.wi + mix(eta, 1.f, is_reflected) * wo);
  m = mulsign(m, cos_theta(m));

  // Compute eta for hero wavelength only, the rest gets killed later on
  // Compute fresnel, angle of transmission
  float cos_theta_t;
  float F = _brdf_dielectric_fresnel(dot(si.wi, m), cos_theta_t, _eta);

  // Evaluate the partial microfacet distribution
  float D_G = eval_microfacet_partial(
    mulsign(si.wi, cos_theta(si.wi)), 
    m, 
    mulsign(wo, cos_theta(wo)), 
    get_dielectric_alpha(brdf)
  );

  if (is_reflected) {
    return vec4(F * D_G) 
      / (4.f * cos_theta(si.wi) * cos_theta(wo));
  } else {
    float scaling  = sdot(inv_eta);
    float jacobian = sdot(eta) * dot(wo, m) / sdot(dot(si.wi, m) + eta * dot(wo, m));

    return vec4(abs(
      /* scaling * */ jacobian * (1.f - F) * D_G * dot(si.wi, m) 
      / (cos_theta(si.wi) * cos_theta(wo)) 
    ));
  }
}

float pdf_brdf_dielectric(in BRDF brdf, in Interaction si, in vec3 wo, in vec4 wvls) {
  bool  is_reflected = cos_theta(si.wi) * cos_theta(wo) >= 0.f;
  float _eta         = _brdf_eta_dispersive(brdf, wvls.x);

  // Get relative index of refraction
  float     eta = cos_theta(si.wi) > 0 ? _eta : 1.f / _eta;
  float inv_eta = cos_theta(si.wi) > 0 ? 1.f / _eta : _eta;

  // Get the half-vector in the positive hemisphere direction
  vec3 m = normalize(si.wi + mix(eta, 1.f, is_reflected) * wo);
  m = mulsign(m, cos_theta(m));

  // Compute fresnel
  float cos_theta_t;
  float F = _brdf_dielectric_fresnel(dot(si.wi, m), cos_theta_t, _eta);

  // Compute jacobian factor
  float jacobian = is_reflected
                 ? (1.f / (4.f * dot(wo, m)))
                 : (sdot(eta) * dot(wo, m) / sdot(dot(si.wi, m) + eta * dot(wo, m)));
  
  float pdf = pdf_microfacet(mulsign(si.wi, cos_theta(si.wi)), m, get_microfacet_alpha(brdf))
            * (is_reflected ? F : 1.f - F)
            * abs(jacobian);
  return pdf;
}

BRDFSample sample_brdf_dielectric(in BRDF brdf, in vec3 sample_3d, in Interaction si, in vec4 wvls) {
  // Sample a microfacet normal to reflect/refract on
  MicrofacetSample ms = sample_microfacet(mulsign(si.wi, cos_theta(si.wi)),
                                          get_microfacet_alpha(brdf), 
                                          sample_3d.yz);

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
    bs.pdf         = ms.pdf * F;

    float jacobian = 1.f / (4.f * dot(bs.wo, ms.n));
    bs.pdf *= abs(jacobian);
  } else {
    // Refract on microfacet normal
    vec3 wo = local_refract(local_wi, cos_theta_t, inv_eta);

    bs.is_spectral = get_dielectric_is_dispersive(brdf);
    bs.wo          = to_world(local_fr, wo);
    bs.pdf         = ms.pdf * (1.f - F);

    float jacobian = sdot(eta) * dot(bs.wo, ms.n)
                   / sdot(dot(si.wi, ms.n) + eta * dot(bs.wo, ms.n));
    bs.pdf *= abs(jacobian);
  }

  return bs;
}

#endif // BRDF_DIELECTRIC_GLSL_GUARD