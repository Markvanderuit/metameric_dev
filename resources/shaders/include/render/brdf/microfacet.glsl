#ifndef BRDF_MICROFACET_GLSL_GUARD
#define BRDF_MICROFACET_GLSL_GUARD

#include <render/record.glsl>
#include <render/warp.glsl>
#include <render/microfacet.glsl>

vec2 _microfacet_lobe_probs(in BRDFInfo brdf, in SurfaceInfo si) {  
  // Estimate fresnel for incident vector to establish probabilities
  vec4 F = schlick_fresnel(brdf.F0, cos_theta(si.wi));
  
  // Diffuse lobe probability mixed by metallic and the average of fresnel 
  // over the four wavelengths. We *do not* deal with differing probabilities, 
  // I don't want to implement hero sampling.
  float prob_diff = (1.f - (hsum(F) * 0.25f)) * (1.f - brdf.metallic);
  float prob_spec = 1.f - prob_diff;
  
  // Normalizes
  float prob_rcp = 1.f / (prob_diff + prob_spec);
  prob_diff *= prob_rcp;
  prob_spec *= prob_rcp;

  // We consistently return [spec, diffuse]
  return vec2(prob_spec, prob_diff);
}

void init_brdf_microfacet(inout BRDFInfo brdf, in SurfaceInfo si, vec4 wvls, in vec2 sample_2d) {
  float eta = 1.28f;
  
  vec2 brdf_data = texture_brdf(si, sample_2d);
  brdf.r        = texture_reflectance(si, wvls, sample_2d);
  brdf.alpha    = max(1e-3, brdf_data.x);
  brdf.metallic = brdf_data.y;
  brdf.F0       = clamp(mix(vec4(eta_to_specular(eta)), brdf.r, brdf.metallic), 0.f, 1.f);
}

vec4 eval_brdf_microfacet(in BRDFInfo brdf, in SurfaceInfo si, in vec3 wo) {
  if (cos_theta(si.wi) <= 0.f || cos_theta(wo) <= 0.f)
    return vec4(0);

  // Output value
  vec4 f = vec4(0);
  
  // Diffuse component
  // Lambert, subdued by metallic for now
  vec4 diffuse = brdf.r * (1.f - brdf.metallic);
  f += diffuse;

  // Specular componennt
  // Evaluate ggx and fresnel
  vec3   wh = normalize(si.wi + wo);
  float D_G = eval_microfacet(si, wh, wo, brdf.alpha);
  vec4  F   = schlick_fresnel(brdf.F0, max(0.f, dot(si.wi, wh)));
  f += D_G * F;

  return f * M_PI_INV;
}

float pdf_brdf_microfacet(in BRDFInfo brdf, in SurfaceInfo si, in vec3 wo) {
  if (cos_theta(si.wi) <= 0.f || cos_theta(wo) <= 0.f)
    return 0.f;

  vec2 lobe_probs = _microfacet_lobe_probs(brdf, si);

  // Output values
  float specular_pdf = lobe_probs[0] * .5f * pdf_microfacet(si, normalize(si.wi + wo), brdf.alpha);
  float diffuse_pdf  = lobe_probs[1] * .5f * square_to_cos_hemisphere_pdf(wo);

  // Mix probabilities [spec, diffuse] for the two lobes
  return (diffuse_pdf + specular_pdf);
}

BRDFSample sample_brdf_microfacet(in BRDFInfo brdf, in vec3 sample_3d, in SurfaceInfo si) {
  if (cos_theta(si.wi) <= 0.f)
    return brdf_sample_zero();

  // Return value
  BRDFSample bs;
  bs.is_delta = false;
  
  // Select a lobe based on sample probabilities [spec, diffuse]
  vec2 lobe_probs = _microfacet_lobe_probs(brdf, si);
  if (sample_3d.x < lobe_probs[0]) { // Specular lobe  microfacet sampling
    // Sample a microfacet normal to reflect with
    MicrofacetSample ms = sample_microfacet(si, brdf.alpha, sample_3d.yz);
    bs.wo = reflect(-si.wi, ms.n);
    if (ms.pdf == 0.f || cos_theta(bs.wo) <= 0.f)
      return brdf_sample_zero();
  } else { // Diffuse lobe hemisphere sampling
    bs.wo  = square_to_cos_hemisphere(sample_3d.yz);
    if (cos_theta(bs.wo) <= 0.f)
      return brdf_sample_zero();
  }

  bs.pdf = lobe_probs[0] * 2.f * pdf_microfacet(si, normalize(si.wi + bs.wo), brdf.alpha)
         + lobe_probs[1] * 2.f * square_to_cos_hemisphere_pdf(bs.wo);

  return bs;
}

#endif // BRDF_MICROFACET_GLSL_GUARD
