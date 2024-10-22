#ifndef BRDF_PRINCIPLED_GLSL_GUARD
#define BRDF_PRINCIPLED_GLSL_GUARD

#include <render/record.glsl>
#include <render/warp.glsl>
#include <render/microfacet.glsl>

void init_brdf_principled(inout BRDFInfo brdf, in SurfaceInfo si, vec4 wvls) {
  // Temporary until I can connect them to parameters
  float roughness = 0.01f;
  float metallic  = 0.0f;
  float eta       = 1.46f;

  brdf.r        = scene_sample_reflectance(record_get_object(si.data), si.tx, wvls);
  brdf.metallic = metallic;
  brdf.alpha    = max(1e-4, sdot(roughness));
  brdf.F0       = clamp(mix(vec4(eta_to_specular(eta)), brdf.r, brdf.metallic), 0.f, 1.f);
}

vec2 _get_principled_lobe_probs(in BRDFInfo brdf, in SurfaceInfo si) {
  // Estimate fresnel for incident vector to establish probabilities
  vec4 F = schlick_fresnel(brdf.F0, cos_theta(si.wi));
  
  // Diffuse lobe probability mixed by metallic and the average of fresnel 
  // over the four wavelengths. We *do not* deal with differing probabilities, 
  // I don't want to implement hero sampling.
  float prob_diff = 0.5f; // (1.f - (hsum(F) * 0.25f)) * (1.f - brdf.metallic);
  float prob_spec = 1.f - prob_diff;

  // We consistently return [spec, diffuse]
  return vec2(prob_spec, prob_diff);
}

vec4 eval_brdf_principled(in BRDFInfo brdf, in SurfaceInfo si, in vec3 wo) {
  if (cos_theta(si.wi) <= 0.f || cos_theta(wo) <= 0.f)
    return vec4(0);

  if (wo.z * si.wi.z <= 0.f)
    return vec4(0);

  // Sample probabilities [spec, diffuse] for the two lobes
  vec2 lobe_probs = vec2(0.5f); // _get_principled_lobe_probs(brdf, si);

  // Output value
  vec4 f = vec4(0);

  // Specular lobe
  if (lobe_probs[0] > 0.f) {
    // Half vector of wi and wo
    vec3 wh = normalize(si.wi + wo);

    // (F D G) / (4 cos(wi) cos(wo))
    // f += schlick_fresnel(brdf.F0, abs(dot(si.wi, wh))) 
    //    * _G(si.wi, wo, brdf.alpha)
    //    / dot(wh, wo);
    f += schlick_fresnel(brdf.F0, abs(dot(si.wi, wh)))
       * _G(si.wi, wo, brdf.alpha)
       / cos_theta(wo);
  }

  // Diffuse lobe
  if (lobe_probs[1] > 0.f) {
    f += brdf.r * M_PI_INV;
  }

  return f;
}

float pdf_brdf_principled(in BRDFInfo brdf, in SurfaceInfo si, in vec3 wo) {
  if (cos_theta(si.wi) <= 0.f ||  cos_theta(wo) <= 0.f)
    return 0.f;

  if (wo.z * si.wi.z <= 0.f)
    return 0.f;

  // Sample probabilities [spec, diffuse] for the two lobes
  vec2 lobe_probs = vec2(0.5f); // _get_principled_lobe_probs(brdf, si);
  
  // Output value
  float pdf = 0.f;

  // Specular lobe
  if (lobe_probs[0] > 0.f) {
    // Half vector of wi and wo
    vec3 wh = normalize(si.wi + wo);
    
    // pdf += lobe_probs[0]
    //      * _G1(si.wi, brdf.alpha);
    pdf += lobe_probs[0] 
         * _G1(si.wi, brdf.alpha); // jacobian
  }

  // Diffuse lobe
  if (lobe_probs[1] > 0.f) {
    pdf += lobe_probs[1] 
         * square_to_cos_hemisphere_pdf(wo);
  }

  return pdf;
}

BRDFSample sample_brdf_principled(in BRDFInfo brdf, in vec3 sample_3d, in SurfaceInfo si) {
  if (cos_theta(si.wi) <= 0.f)
    return brdf_sample_zero();

  BRDFSample bs;
  bs.is_delta = false;

  // Select a lobe based on sample probabilities [spec, diffuse]
  vec2 lobe_probs = vec2(0.5f); // _get_principled_lobe_probs(brdf, si);
  if (sample_3d.x < lobe_probs[0]) { // Rough specular lobe
    // Sample a microfacet normal
    MicrofacetSample ms = sample_microfacet(si.wi, brdf.alpha, sample_3d.yz);
    if (ms.pdf == 0.f)
      return brdf_sample_zero();
    
    // Do a specular reflection off the sample normal
    bs.wo = reflect(-si.wi, ms.n);
    
    // Validate same hemisphere
    if (bs.wo.z * si.wi.z <= 0.f)
      return brdf_sample_zero();

    // Validate exitant angle
    if (cos_theta(bs.wo) <= 0.f || ms.pdf == 0.f)
      return brdf_sample_zero();

    // Most of f/pdf is canceled out
    // bs.pdf += lobe_probs[0] 
    //         * ms.pdf;
  } else { // Diffuse lobe
    bs.wo  = square_to_cos_hemisphere(sample_3d.yz);
    
    // Validate exitant angle
    if (cos_theta(bs.wo) <= 0.f)
      return brdf_sample_zero();

    // bs.pdf += lobe_probs[1]
    //         * square_to_cos_hemisphere_pdf(bs.wo);
  }

  bs.f   = eval_brdf_principled(brdf, si, bs.wo);
  bs.pdf = pdf_brdf_principled(brdf, si, bs.wo);

  /* // Handle specular throughput
  if (lobe_probs[0] > 0.f) {
    // Half vector of wi and wo
    vec3 wh = normalize(si.wi + bs.wo);

    // float G = _G(si.wi, bs.wo, brdf.alpha);
    // vec4  F = schlick_fresnel(brdf.F0, abs(dot(si.wi, wh))); // recalculate for new normal
    
    // bs.f += (F * G) / cos_theta(bs.wo);
    bs.f += schlick_fresnel(brdf.F0, abs(dot(si.wi, wh)))
          * eval_microfacet(si.wi, wh, bs.wo, brdf.alpha);
  }

  // Handle diffuse throughput
  if (lobe_probs[1] > 0.f) {
    bs.f += brdf.r * M_PI_INV;
  } */

  return bs;
}

#endif // BRDF_PRINCIPLED_GLSL_GUARD
