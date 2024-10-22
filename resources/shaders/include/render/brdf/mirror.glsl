#ifndef BRDF_MIRROR_GLSL_GUARD
#define BRDF_MIRROR_GLSL_GUARD

#include <render/record.glsl>

void init_brdf_mirror(inout BRDFInfo brdf, in SurfaceInfo si, vec4 wvls) {
  brdf.r = scene_sample_reflectance(record_get_object(si.data), si.tx, wvls);
}

BRDFSample sample_brdf_mirror(in BRDFInfo brdf, in vec3 sample_3d, in SurfaceInfo si) {
  BRDFSample bs;

  if (cos_theta(si.wi) <= 0.f) {
    bs.pdf = 0.f;
    return bs;
  }

  bs.is_delta = true;
  bs.wo       = vec3(-si.wi.xy, si.wi.z);
  bs.f        = schlick_fresnel(brdf.r, cos_theta(si.wi)) 
              / cos_theta(bs.wo);
  bs.pdf      = 1.f;

  return bs;
}

vec4 eval_brdf_mirror(in BRDFInfo brdf, in SurfaceInfo si, in vec3 wo) {
  return vec4(0);
}

float pdf_brdf_mirror(in BRDFInfo brdf, in SurfaceInfo si, in vec3 wo) {
  return 0.f;
}

#endif // BRDF_MIRROR_GLSL_GUARD