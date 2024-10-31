#ifndef BRDF_MIRROR_GLSL_GUARD
#define BRDF_MIRROR_GLSL_GUARD

#include <render/record.glsl>

void init_brdf_mirror(inout BRDFInfo brdf, in SurfaceInfo si, vec4 wvls, in vec2 sample_2d) {
  brdf.r = texture_reflectance(si, wvls, sample_2d);
}

BRDFSample sample_brdf_mirror(in BRDFInfo brdf, in vec3 sample_3d, in SurfaceInfo si) {
  BRDFSample bs;

  if (cos_theta(si.wi) <= 0.f)
    return brdf_sample_zero();

  bs.is_delta = true;
  bs.wo       = vec3(-si.wi.xy, si.wi.z);
  bs.pdf      = 1.f;

  return bs;
}

vec4 eval_brdf_mirror(in BRDFInfo brdf, in SurfaceInfo si, in vec3 wo) {
  return schlick_fresnel(brdf.r, cos_theta(si.wi)) / cos_theta(wo);
}

float pdf_brdf_mirror(in BRDFInfo brdf, in SurfaceInfo si, in vec3 wo) {
  return 0.f;
}

#endif // BRDF_MIRROR_GLSL_GUARD