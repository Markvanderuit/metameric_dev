#ifndef BRDF_DIELECTRIC_GLSL_GUARD
#define BRDF_DIELECTRIC_GLSL_GUARD

#include <render/record.glsl>
#include <render/ggx.glsl>
#include <render/fresnel.glsl>

vec4 eval_brdf_dielectric(in BRDF brdf, in Interaction si, in vec3 wo, in vec4 wvls) {
  bool is_reflected = cos_theta(si.wi) * cos_theta(wo) >= 0.f;

  // Get relative index of refraction along ray
  float     eta = cos_theta(si.wi) > 0 ? brdf.eta : 1.f / brdf.eta;
  float inv_eta = cos_theta(si.wi) > 0 ? 1.f / brdf.eta : brdf.eta;

  // Get the half-vector in the positive hemisphere direction as acting normal
  vec3 m = to_upper_hemisphere(normalize(si.wi + wo * (is_reflected ? 1.f : eta)));

  // Evaluate fresnel and the microfacet distribution
  float F  = schlick_fresnel(schlick_F0(brdf.eta), dot(to_upper_hemisphere(si.wi), m));
  float GD = eval_ggx(si.wi, m, wo, brdf.alpha);

  if (is_reflected) {
    float weight = 1.f / (4.f * cos_theta(si.wi) * cos_theta(wo));
    return vec4(F * GD * abs(weight));
  } else {
    float weight = sdot(inv_eta) * dot(si.wi, m) * dot(wo, m) 
                 / abs(sdot(dot(wo, m) + dot(si.wi, m) * inv_eta) * cos_theta(si.wi) * cos_theta(wo));
    return vec4((1.f - F) * GD * abs(weight));
  }
}

float pdf_brdf_dielectric(in BRDF brdf, in Interaction si, in vec3 wo, in vec4 wvls) {
  bool is_reflected = cos_theta(si.wi) * cos_theta(wo) >= 0.f;

  // Get relative index of refraction
  float     eta = cos_theta(si.wi) > 0 ? brdf.eta : 1.f / brdf.eta;
  float inv_eta = cos_theta(si.wi) > 0 ? 1.f / brdf.eta : brdf.eta;

  // Get the half-vector in the positive hemisphere direction as acting normal
  vec3 m = to_upper_hemisphere(normalize(si.wi + mix(eta, 1.f, is_reflected) * wo));

  // Compute fresnel
  float F = schlick_fresnel(schlick_F0(brdf.eta), dot(to_upper_hemisphere(si.wi), m));
  
  float pdf = pdf_ggx(si.wi, m, brdf.alpha);
  if (is_reflected) {
    float weight = 1.f / (4.f * dot(si.wi, m));
    return pdf * F * abs(weight);
  } else {
    float weight = sdot(inv_eta) * dot(wo, m) 
                 / sdot(dot(wo, m) + dot(si.wi, m) * inv_eta);
    return pdf * (1.f - F) * abs(weight);
  }
}

BRDFSample sample_brdf_dielectric(in BRDF brdf, in vec3 sample_3d, in Interaction si, in vec4 wvls) {
  // Sample a microfacet normal to reflect/refract on
  MicrofacetSample ms = sample_ggx(si.wi, brdf.alpha, sample_3d.yz);
  
  // Get relative index of refraction
  float     eta = cos_theta(si.wi) > 0 ? brdf.eta : 1.f / brdf.eta;
  float inv_eta = cos_theta(si.wi) > 0 ? 1.f / brdf.eta : brdf.eta;

  // Move into microfacet normal's frame
  Frame local_fr = get_frame(ms.n);
  vec3  local_wi = to_local(local_fr, si.wi);
  
  // Return object
  BRDFSample bs;
  bs.is_delta = false;
  bs.pdf      = ms.pdf;

  // Compute fresnel and angle of transmission; F is set to 1 on total internal reflection
  float cos_theta_t;
  float F = schlick_fresnel(schlick_F0(brdf.eta), cos_theta(local_wi), cos_theta_t, brdf.eta);
  
  // Pick reflection/refraction lobe
  if (sample_3d.x < F) {
    // Reflect on microfacet normal
    vec3 local_wo = local_reflect(local_wi);
    float weight  = 1.f / (4.f * cos_theta(local_wi));

    bs.is_spectral = false;
    bs.wo          = to_world(local_fr, local_wo);
    bs.pdf         = F * ms.pdf * abs(weight);
  } else {
    // Refract on microfacet normal
    vec3 local_wo = local_refract(local_wi, cos_theta_t, inv_eta);
    float weight  = sdot(inv_eta) * cos_theta(local_wo)
                  / sdot(cos_theta(local_wo) + cos_theta(local_wi) * inv_eta);
    
    bs.is_spectral = brdf.is_spectral;
    bs.wo          = to_world(local_fr, local_wo);
    bs.pdf         = (1.f - F) * ms.pdf * abs(weight);
  }

  return bs;
}

#endif // BRDF_DIELECTRIC_GLSL_GUARD