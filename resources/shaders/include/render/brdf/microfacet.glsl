#ifndef BRDF_MICROFACET_GLSL_GUARD
#define BRDF_MICROFACET_GLSL_GUARD

#include <render/record.glsl>
#include <render/warp.glsl>
#include <render/microfacet.glsl>

// Accessors to BRDFInfo data
#define get_microfacet_r(brdf)        brdf.r
#define get_microfacet_alpha(brdf)    brdf.data.x
#define get_microfacet_metallic(brdf) brdf.data.y
#define get_microfacet_eta(brdf)      brdf.data.z

vec2 _microfacet_lobe_probs(in BRDFInfo brdf, in SurfaceInfo si) {  
  // Estimate fresnel for incident vector to establish probabilities
  vec4 F0 = clamp(mix(vec4(eta_to_specular(get_microfacet_eta(brdf))), get_microfacet_r(brdf), get_microfacet_metallic(brdf)), 0.f, 1.f);
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

void init_brdf_microfacet(in ObjectInfo object, inout BRDFInfo brdf, in SurfaceInfo si, vec4 wvls, in vec2 sample_2d) {
  vec2 brdf_data = texture_brdf(si, sample_2d);
  get_microfacet_r(brdf)         = texture_reflectance(si, wvls, sample_2d);
  get_microfacet_alpha(brdf)     = max(1e-3, brdf_data.x);
  get_microfacet_metallic(brdf)  = brdf_data.y;
  get_microfacet_eta(brdf)       = object.eta_minmax.x;
}

vec4 eval_brdf_microfacet(in BRDFInfo brdf, in SurfaceInfo si, in vec3 wo) {
  if (cos_theta(si.wi) <= 0.f || cos_theta(wo) <= 0.f)
    return vec4(0);

  // Output value
  vec4 f = vec4(0);
  
  // Diffuse component
  // Lambert, subdued by metallic for now
  vec4 diffuse = (1.f - get_microfacet_metallic(brdf))
               * get_microfacet_r(brdf);
  f += diffuse;

  // Specular componennt
  // Evaluate ggx and fresnel
  vec3   wh = normalize(si.wi + wo);
  float D_G = eval_microfacet(si, wh, wo, get_microfacet_alpha(brdf));
  vec4  F0  = clamp(mix(vec4(eta_to_specular(get_microfacet_eta(brdf))), get_microfacet_r(brdf), get_microfacet_metallic(brdf)), 0.f, 1.f);
  vec4  F   = schlick_fresnel(F0, max(0.f, dot(si.wi, wh)));
  vec4 specular = D_G * F;
  f += specular;

  return f * M_PI_INV;
}

float pdf_brdf_microfacet(in BRDFInfo brdf, in SurfaceInfo si, in vec3 wo) {
  if (cos_theta(si.wi) <= 0.f || cos_theta(wo) <= 0.f)
    return 0.f;

  vec2 lobe_probs = _microfacet_lobe_probs(brdf, si);

  // Output values
  float specular_pdf = lobe_probs[0] * .5f * pdf_microfacet(si, normalize(si.wi + wo), get_microfacet_alpha(brdf));
  float diffuse_pdf  = lobe_probs[1] * .5f * square_to_cos_hemisphere_pdf(wo);

  // Mix probabilities [spec, diffuse] for the two lobes
  return (diffuse_pdf + specular_pdf);
}

BRDFSample sample_brdf_microfacet(in BRDFInfo brdf, in vec3 sample_3d, in SurfaceInfo si) {
  if (cos_theta(si.wi) <= 0.f)
    return brdf_sample_zero();

  // Return value
  BRDFSample bs;
  bs.is_spectral = false;
  bs.is_delta = false;
  
  // Select a lobe based on sample probabilities [spec, diffuse]
  vec2 lobe_probs = _microfacet_lobe_probs(brdf, si);
  if (sample_3d.x < lobe_probs[0]) { // Specular lobe  microfacet sampling
    // Sample a microfacet normal to reflect with
    MicrofacetSample ms = sample_microfacet(si, get_microfacet_alpha(brdf), sample_3d.yz);
    bs.wo = reflect(-si.wi, ms.n);
    if (ms.pdf == 0.f || cos_theta(bs.wo) <= 0.f)
      return brdf_sample_zero();
  } else { // Diffuse lobe hemisphere sampling
    bs.wo  = square_to_cos_hemisphere(sample_3d.yz);
    if (cos_theta(bs.wo) <= 0.f)
      return brdf_sample_zero();
  }

  bs.pdf = lobe_probs[0] * 2.f * pdf_microfacet(si, normalize(si.wi + bs.wo), get_microfacet_alpha(brdf))
         + lobe_probs[1] * 2.f * square_to_cos_hemisphere_pdf(bs.wo);

  return bs;
}

#endif // BRDF_MICROFACET_GLSL_GUARD
