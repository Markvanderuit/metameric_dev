#ifndef BRDF_PRINCIPLED_GLSL_GUARD
#define BRDF_PRINCIPLED_GLSL_GUARD

#include <render/record.glsl>
#include <render/warp.glsl>
#include <render/microfacet.glsl>

// Debug flags
const bool _brdf_enable_diffuse  = true;
const bool _brdf_enable_specular = true;

void init_brdf_principled(inout BRDFInfo brdf, in SurfaceInfo si, vec4 wvls) {
  // Temporary until I can connect them to scene sparameters
  float roughness = 0.03f;
  float metallic  = 1.0f;
  float eta       = 1.46f;
  
  brdf.r        = scene_sample_reflectance(record_get_object(si.data), si.tx, wvls);
  brdf.metallic = metallic;
  brdf.alpha    = max(1e-3, sdot(roughness)); // clamped to 0.001 to prevent issuess
  brdf.F0       = clamp(mix(vec4(eta_to_specular(eta)), brdf.r, brdf.metallic), 0.f, 1.f);
}

vec2 _get_principled_lobe_probs(in BRDFInfo brdf, in SurfaceInfo si) {  
  return vec2(1, 0);

  // Estimate fresnel for incident vector to establish probabilities
  vec4 F = schlick_fresnel(brdf.F0, vec4(1), cos_theta(si.wi));
  
  // Diffuse lobe probability mixed by metallic and the average of fresnel 
  // over the four wavelengths. We *do not* deal with differing probabilities, 
  // I don't want to implement hero sampling.
  float prob_diff = (1.f - (hsum(F) * 0.25f)) * (1.f - brdf.metallic);
  if (!_brdf_enable_diffuse)
    prob_diff = 0.f;

  float prob_spec = 1.f - prob_diff;
  if (!_brdf_enable_specular)
    prob_spec = 0.f;
  
  // Normalizes
  float prob_rcp = 1.f / (prob_diff + prob_spec);
  prob_diff *= prob_rcp;
  prob_spec *= prob_rcp;

  // We consistently return [spec, diffuse]
  return vec2(prob_spec, prob_diff);
}

vec4 eval_brdf_principled(in BRDFInfo brdf, in SurfaceInfo si, in vec3 wo) {
  if (cos_theta(si.wi) <= 0.f || cos_theta(wo) <= 0.f)
    return vec4(0);

  // Output value
  vec4 f = vec4(0);

  // Diffuse component
  /* if (_brdf_enable_diffuse) */ 
  {
    // // Disney diffuse
    // float F90             = sdot(dot(wh, si.wi)) * 2.f * brdf.alpha + .5f; // TODO that sqrt is wholly unnecessary
    // float diffuse_fresnel =  // TODO single-component math implementation
    //   schlick_fresnel(vec4(1), vec4(F90), cos_theta(si.wi)).x *
    //   schlick_fresnel(vec4(1), vec4(F90), cos_theta(wo)).x;
    // vec4 diffuse = brdf.r * diffuse_fresnel * (1.f - brdf.metallic);

    // Lambert, hacked with metallic for now
    /* vec4 lambert = brdf.r * (1.f - brdf.metallic);

    f += lambert; */
  }

  // Half vector of wi and wo
  vec3 wh = normalize(si.wi + wo);
  
  // Microfacet specular component
  // Evaluate microfacet throughput and fresnel
  float D_G = eval_microfacet(si, wh, wo, brdf.alpha);
  vec4  F   = schlick_fresnel(brdf.F0, max(0.f, dot(si.wi, wh)));
  f += D_G * F;

  return f  * M_PI_INV;
}

float pdf_brdf_principled(in BRDFInfo brdf, in SurfaceInfo si, in vec3 wo) {
  if (cos_theta(si.wi) <= 0.f || cos_theta(wo) <= 0.f)
    return 0.f;

  // Output values
  float diffuse_pdf  = square_to_cos_hemisphere_pdf(wo);
  float specular_pdf = pdf_microfacet(si, normalize(si.wi + wo), brdf.alpha);

  // Mix probabilities [spec, diffuse] for the two lobes
  return specular_pdf * 0.5f /* * 2.f * M_PI_INV */;
  // return mix(diffuse_pdf, specular_pdf, _get_principled_lobe_probs(brdf, si).x);
}

BRDFSample sample_brdf_principled(in BRDFInfo brdf, in vec3 sample_3d, in SurfaceInfo si) {
  if (cos_theta(si.wi) <= 0.f)
    return brdf_sample_zero();

  // Return value
  BRDFSample bs;
  bs.is_delta = false;
  
  // Select a lobe based on sample probabilities [spec, diffuse]
  vec2 lobe_probs = _get_principled_lobe_probs(brdf, si);
  if (sample_3d.x < lobe_probs[0]) { // Specular lobe  microfacet sampling
    // Sample a microfacet normal to reflect with
    MicrofacetSample ms = sample_microfacet(si, brdf.alpha, sample_3d.yz);
    bs.wo = reflect(-si.wi, ms.n);

    // Validate exitant angle
    if (ms.pdf == 0.f || cos_theta(bs.wo) <= 0.f)
      return brdf_sample_zero();
    
    bs.pdf =  /* 2.f * lobe_probs[0] * */ ms.pdf * 2.f /* * 2.f */ /* * M_PI_INV */ /*  * 4.f * cos_theta(bs.wo) */;
  } else { // Diffuse lobe hemisphere sampling
    bs.wo  = square_to_cos_hemisphere(sample_3d.yz);
    
    // Validate exitant angle
    if (cos_theta(bs.wo) <= 0.f)
      return brdf_sample_zero();

    bs.pdf = lobe_probs[1] * square_to_cos_hemisphere_pdf(bs.wo);
  }

  return bs;
}

#endif // BRDF_PRINCIPLED_GLSL_GUARD
