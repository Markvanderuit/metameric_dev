#ifndef BRDF_DIFFUSE_GLSL_GUARD
#define BRDF_DIFFUSE_GLSL_GUARD

#include <render/warp.glsl>
#include <render/record.glsl>

BRDFSample sample_brdf_diffuse(in BRDF brdf, in vec3 sample_3d, in Interaction si, in vec4 wvls) {
  if (cos_theta(si.wi) <= 0.f)
    return brdf_sample_zero();

  BRDFSample bs;
  bs.is_spectral = false;
  bs.is_delta    = false;
  bs.wo          = square_to_cos_hemisphere(sample_3d.yz);
  bs.pdf         = square_to_cos_hemisphere_pdf(bs.wo);
  
  return bs;
}

vec4 eval_brdf_diffuse(in BRDF brdf, in Interaction si, in vec3 wo, in vec4 wvls) {
  if (cos_theta(si.wi) <= 0.f || cos_theta(wo) <= 0.f)
    return vec4(0.f);
  
  return brdf.r * M_PI_INV;
}

float pdf_brdf_diffuse(in BRDF brdf, in Interaction si, in vec3 wo) {
  if (cos_theta(si.wi) <= 0.f || cos_theta(wo) <= 0.f)
    return 0.f;
  
  return square_to_cos_hemisphere_pdf(wo);
}

#endif // BRDF_DIFFUSE_GLSL_GUARD