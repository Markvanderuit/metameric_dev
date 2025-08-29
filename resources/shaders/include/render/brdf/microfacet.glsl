#ifndef BRDF_MICROFACET_GLSL_GUARD
#define BRDF_MICROFACET_GLSL_GUARD

#include <render/record.glsl>
#include <render/ggx.glsl>
#include <render/fresnel.glsl>

/* vec2 _microfacet_lobe_probs(in BRDF brdf, in Interaction si) {  
  // Estimate fresnel for incident vector to establish probabilities
  vec4 F0 = mix(vec4(schlick_F0(brdf.eta)), brdf.r, brdf.metallic);
  vec4 F  = schlick_fresnel(F0, cos_theta(si.wi));
  
  // Diffuse lobe probability mixed by metallic and the average of fresnel 
  // over the four wavelengths. We *do not* deal with differing probabilities, 
  // I don't want to implement hero sampling.
  float prob_diff = (1.f - (hsum(F) * 0.25f)) * (1.f - brdf.metallic);
  float prob_spec = 1.f - prob_diff;
  
  // Normalize
  float prob_rcp = 1.f / (prob_diff + prob_spec);
  prob_diff *= prob_rcp;
  prob_spec *= prob_rcp;

  // We consistently return [spec, diffuse]
  return vec2(prob_spec, prob_diff);
} */

// Weighting of different BxDF lobes, potentially shared between BxDF models;
// also caching F because we'll reuse it a lot
struct LobeDensity {
  vec4  F;
  float diffuse_reflect;
  float specular_reflect;
  float specular_refract;
};

LobeDensity detail_get_lobes(in BRDF brdf, in Interaction si, in vec3 wh) {
  // Return value
  LobeDensity lobe;

  // Compute fresnel, assuming a microfacet normal is already available
  vec4 F0 = mix(vec4(schlick_F0(brdf.eta)), brdf.r, brdf.metallic);
  lobe.F  = schlick_fresnel(vec4(F0), vec4(1), dot(to_upper_hemisphere(si.wi), wh)); 
  
  // Average used for metals, and same as F for dielectrics.
  float F_avg = hsum(lobe.F) * .25f;

  if (is_upper_hemisphere(si.wi)) {
    // From upper
    lobe.specular_reflect = F_avg;
    lobe.specular_refract = (1.f - F_avg) * (1.f - brdf.metallic) * brdf.transmission;
    lobe.diffuse_reflect  = (1.f - F_avg) * (1.f - brdf.metallic) * (1.f - brdf.transmission);
  } else {
    // From lower, we don't apply transmission twice, and metallic doesn't even get here
    lobe.specular_reflect = F_avg;
    lobe.specular_refract = 1.f - F_avg;
    lobe.diffuse_reflect  = 0.f;
  }

  // Normalize density
  float rcp = 1.f / (lobe.diffuse_reflect + lobe.specular_reflect + lobe.specular_refract);
  lobe.diffuse_reflect  *= rcp;
  lobe.specular_reflect *= rcp;
  lobe.specular_refract *= rcp;

  return lobe;
}

vec4 eval_brdf_microfacet(in BRDF brdf, in Interaction si, in vec3 wo, in vec4 wvls) {
  bool is_upper     = is_upper_hemisphere(si.wi);
  bool is_reflected = cos_theta(si.wi) * cos_theta(wo) >= 0.f;

  // Get relative index of refraction along ray
  float     eta = is_upper_hemisphere(si.wi) ? brdf.eta : 1.f / brdf.eta;
  float inv_eta = is_upper_hemisphere(si.wi) ? 1.f / brdf.eta : brdf.eta;
  
  // Get the half-vector in the positive hemisphere direction
  vec3 m = to_upper_hemisphere(normalize(si.wi + wo * (is_reflected ? 1.f : eta)));

  // Evaluate fresnel and the microfacet distribution
  vec4 F0  = mix(vec4(schlick_F0(brdf.eta)), brdf.r, brdf.metallic);
  vec4 F   = schlick_fresnel(vec4(F0), vec4(1), dot(to_upper_hemisphere(si.wi), m)); 
  float GD = eval_ggx(si.wi, m, wo, brdf.alpha);

  // Output value
  vec4 f = vec4(0);

  // Diffuse component; lambert scaled by metallic
  if (is_upper && is_reflected) {
    f += (1.f - brdf.metallic) * (1.f - brdf.transmission) * brdf.r * M_PI_INV;
  }

  // Specular reflect; evaluate ggx and multiply by fresnel, jacobian
  if (is_reflected) {
    float weight = 1.f / (4.f * cos_theta(si.wi) * cos_theta(wo));
    f += F * GD * abs(weight);
  }
  
  // Specular refract; evaluate ggx and multiply by fresnel, jacobian
  if (!is_reflected) {
    float denom = sdot(dot(wo, m) + dot(si.wi, m) * inv_eta) * cos_theta(si.wi) * cos_theta(wo);
    float weight = sdot(inv_eta) * dot(si.wi, m) * dot(wo, m) / abs(denom);
    vec4 r = is_upper ? vec4(1) : brdf.r;
    f += brdf.transmission * r * (1.f - F) * GD * abs(weight);
  }

  return f;
}

float pdf_brdf_microfacet(in BRDF brdf, in Interaction si, in vec3 wo) {
  bool is_reflected = cos_theta(si.wi) * cos_theta(wo) >= 0.f;
  bool is_upper     = is_upper_hemisphere(si.wi);

  // Get relative index of refraction
  float     eta = is_upper_hemisphere(si.wi) ? brdf.eta : 1.f / brdf.eta;
  float inv_eta = is_upper_hemisphere(si.wi) ? 1.f / brdf.eta : brdf.eta;

  // Get the half-vector in the positive hemisphere direction
  vec3        m     = to_upper_hemisphere(normalize(si.wi + mix(eta, 1.f, is_reflected) * wo));
  LobeDensity lobes = detail_get_lobes(brdf, si, m);

  // Output value
  float pdf = 0.f;

  // Diffuse lobe sample density
  if (is_reflected && is_upper) {
    pdf += lobes.diffuse_reflect * square_to_cos_hemisphere_pdf(wo);
  }

  // Microfacet sample density
  float GD = pdf_ggx(si.wi, m, brdf.alpha);

  // Reflect lobe sample density
  if (is_reflected) {
    float weight = 1.f / (4.f * dot(si.wi, m));
    pdf += lobes.specular_reflect * GD * abs(weight);
  }

  // Refract lobe sample density
  if (!is_reflected) {
    float weight = sdot(inv_eta) * dot(wo, m) 
                 / sdot(dot(wo, m) + dot(si.wi, m) * inv_eta);
    pdf += lobes.specular_refract * GD * abs(weight);
  }

  return pdf;
}

BRDFSample sample_brdf_microfacet(in BRDF brdf, in vec3 sample_3d, in Interaction si, in vec4 wvls) {
  // Sample a microfacet normal to operate on
  MicrofacetSample ms = sample_ggx(si.wi, brdf.alpha, sample_3d.yz);
  
  // Get relative index of refraction
  float     eta = is_upper_hemisphere(si.wi) ? brdf.eta : 1.f / brdf.eta;
  float inv_eta = is_upper_hemisphere(si.wi) ? 1.f / brdf.eta : brdf.eta;

  // Move into microfacet normal's frame
  Frame local_fr = get_frame(ms.n);
  vec3  local_wi = to_local(local_fr, si.wi);

  // Return object
  BRDFSample bs;
  bs.is_delta    = false;
  bs.is_spectral = false;

  // Compute fresnel, lobe selection density
  LobeDensity lobes = detail_get_lobes(brdf, si, ms.n);

  // Sample one of the lobes 
  if (sample_3d.x < lobes.specular_reflect) {
    // Reflect on microfacet normal
    bs.wo          = to_world(local_fr, local_reflect(local_wi));
    bs.is_spectral = false;
    
    if (cos_theta(bs.wo) * cos_theta(si.wi) <= 0)
      return brdf_sample_zero();
  } else if (sample_3d.x < (lobes.specular_reflect + lobes.specular_refract)) {
    // Refract on microfacet normal
    bs.wo          = to_world(local_fr, local_refract(local_wi, inv_eta));
    bs.is_spectral = brdf.is_spectral;
    
    if (cos_theta(bs.wo) * cos_theta(si.wi) >= 0)
      return brdf_sample_zero();
  } else if (sample_3d.x < (lobes.specular_reflect + lobes.specular_refract + lobes.diffuse_reflect)) {
    // Sample diffuse instead
    bs.wo          = square_to_cos_hemisphere(sample_3d.yz);
    bs.is_spectral = false;

    if (cos_theta(bs.wo) * cos_theta(si.wi) <= 0)
      return brdf_sample_zero();
  }

  bs.pdf = pdf_brdf_microfacet(brdf, si, bs.wo);

  return bs;
}

#endif // BRDF_MICROFACET_GLSL_GUARD
