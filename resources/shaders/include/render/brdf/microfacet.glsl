#ifndef BRDF_MICROFACET_GLSL_GUARD
#define BRDF_MICROFACET_GLSL_GUARD

#include <render/record.glsl>
#include <render/ggx.glsl>
#include <render/fresnel.glsl>

// Accessors to BRDF data
#define get_microfacet_r(brdf)        brdf.r
#define get_microfacet_alpha(brdf)    brdf.data.x
#define get_microfacet_metallic(brdf) brdf.data.y
#define get_microfacet_eta(brdf)      brdf.data.z

vec2 _microfacet_lobe_probs(in BRDF brdf, in Interaction si) {  
  // Estimate fresnel for incident vector to establish probabilities
  vec4 F0 = mix(vec4(sdot(get_microfacet_eta(brdf) - 1) / sdot(get_microfacet_eta(brdf) + 1) / 0.08f),
                get_microfacet_r(brdf),
                get_microfacet_metallic(brdf));
  vec4 F = schlick_fresnel(F0, cos_theta(si.wi));
  
  // Diffuse lobe probability mixed by metallic and the average of fresnel 
  // over the four wavelengths. We *do not* deal with differing probabilities, 
  // I don't want to implement hero sampling.
  float prob_diff = (1.f - (hsum(F) * 0.25f)) * (1.f - get_microfacet_metallic(brdf));
  float prob_spec = 1.f - prob_diff;
  
  // Normalizes
  float prob_rcp = 1.f / (prob_diff + prob_spec);
  prob_diff *= prob_rcp;
  prob_spec *= prob_rcp;

  // We consistently return [spec, diffuse]
  return vec2(prob_spec, prob_diff);
}

vec4 eval_brdf_microfacet(in BRDF brdf, in Interaction si, in vec3 wo, in vec4 wvls) {
  if (cos_theta(si.wi) <= 0.f || cos_theta(wo) <= 0.f)
    return vec4(0);

  vec3 wh = normalize(si.wi + wo);
  vec4 F0 = mix(vec4(sdot(get_microfacet_eta(brdf) - 1) / sdot(get_microfacet_eta(brdf) + 1) / 0.08f),
                get_microfacet_r(brdf),
                get_microfacet_metallic(brdf));
  vec4 F = schlick_fresnel(vec4(F0), vec4(1), dot(si.wi, wh)); 

  // Output value
  vec4 f = vec4(0);
  
  // Diffuse component
  // Lambert scaled by metallic
  {
    f += (1.f - get_microfacet_metallic(brdf))
       * get_microfacet_r(brdf)
       * M_PI_INV;
  }

  // Specular componennt
  // Evaluate ggx and multiply by fresnel
  {
    float D_G    = eval_ggx(si.wi, wh, wo, get_microfacet_alpha(brdf));
    float weight = 1.f / (4.f * cos_theta(si.wi) * cos_theta(wo));
    f += F * D_G * weight;
  }

  return f;
}

float pdf_brdf_microfacet(in BRDF brdf, in Interaction si, in vec3 wo) {
  if (cos_theta(si.wi) <= 0.f || cos_theta(wo) <= 0.f)
    return 0.f;
  
  vec3 wh  = normalize(si.wi + wo);
  vec2 lobe_probs = _microfacet_lobe_probs(brdf, si);

  // Output values
  float specular_pdf = lobe_probs[0] * pdf_ggx(si.wi, wh, get_microfacet_alpha(brdf))
                     / (4.f * cos_theta(si.wi) /* * dot(si.wi, wh) */);
  float diffuse_pdf  = lobe_probs[1] * square_to_cos_hemisphere_pdf(wo);

  // Mix probabilities [spec, diffuse] for the two lobes
  return diffuse_pdf + specular_pdf;
}

BRDFSample sample_brdf_microfacet(in BRDF brdf, in vec3 sample_3d, in Interaction si, in vec4 wvls) {
  if (cos_theta(si.wi) <= 0.f)
    return brdf_sample_zero();
  
  // Return object
  BRDFSample bs;
  bs.is_spectral = false;
  bs.is_delta    = false;
  
  // Select a lobe based on sample probabilities [spec, diffuse]
  vec2 lobe_probs = _microfacet_lobe_probs(brdf, si);
  if (sample_3d.x < lobe_probs[0]) { // Specular lobe  microfacet sampling
    // Sample a microfacet normal to reflect with
    MicrofacetSample ms = sample_ggx(si.wi, get_microfacet_alpha(brdf), sample_3d.yz);
    bs.wo = reflect(-si.wi, ms.n);

    if (ms.pdf == 0.f || cos_theta(bs.wo) <= 0.f)
      return brdf_sample_zero();
  } else { // Diffuse lobe hemisphere sampling
    bs.wo  = square_to_cos_hemisphere(sample_3d.yz);

    if (cos_theta(bs.wo) <= 0.f)
      return brdf_sample_zero();
  }
  
  bs.pdf = pdf_brdf_microfacet(brdf, si, bs.wo);

  return bs;
}

#endif // BRDF_MICROFACET_GLSL_GUARD
