#ifndef BRDF_DIFFUSE_GLSL_GUARD
#define BRDF_DIFFUSE_GLSL_GUARD

#include <render/warp.glsl>
#include <render/record.glsl>

void init_brdf_diffuse(inout BRDFInfo brdf, in SurfaceInfo si, vec4 wvls, in vec2 sample_2d) {
  brdf.r = scene_sample_reflectance_stochastic(record_get_object(si.data), si.tx, wvls, sample_2d);
  // brdf.r = scene_sample_reflectance(record_get_object(si.data), si.tx, wvls);
  // brdf.r = vec4(1);
}

BRDFSample sample_brdf_diffuse(in BRDFInfo brdf, in vec3 sample_3d, in SurfaceInfo si) {
  if (cos_theta(si.wi) <= 0.f)
    return brdf_sample_zero();

  BRDFSample bs;
  bs.is_delta = false;
  bs.wo       = square_to_cos_hemisphere(sample_3d.yz);
  bs.pdf      = square_to_cos_hemisphere_pdf(bs.wo);
  return bs;
}

vec4 eval_brdf_diffuse(in BRDFInfo brdf, in SurfaceInfo si, in vec3 wo) {
  if (cos_theta(si.wi) <= 0.f || cos_theta(wo) <= 0.f)
    return vec4(0.f);
  return brdf.r * M_PI_INV;
}

float pdf_brdf_diffuse(in BRDFInfo brdf, in SurfaceInfo si, in vec3 wo) {
  if (cos_theta(si.wi) <= 0.f || cos_theta(wo) <= 0.f)
    return 0.f;
  return square_to_cos_hemisphere_pdf(wo);
}

#endif // BRDF_DIFFUSE_GLSL_GUARD