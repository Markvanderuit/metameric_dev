#ifndef BRDF_DIFFUSE_GLSL_GUARD
#define BRDF_DIFFUSE_GLSL_GUARD

#include <render/warp.glsl>
#include <render/record.glsl>

void init_brdf_diffuse(inout BRDFInfo brdf, in SurfaceInfo si, vec4 wvls) {
  brdf.r = scene_sample_reflectance(record_get_object(si.data), si.tx, wvls);
}

BRDFSample sample_brdf_diffuse(in BRDFInfo brdf, in vec2 sample_2d, in SurfaceInfo si) {
  BRDFSample bs;

  if (cos_theta(si.wi) <= 0.f) {
    bs.pdf = 0.f;
    return bs;
  }

  bs.is_delta = false;
  bs.f        = brdf.r * M_PI_INV;
  bs.wo       = square_to_cos_hemisphere(sample_2d);
  bs.pdf      = square_to_cos_hemisphere_pdf(bs.wo);

  return bs;
}

vec4 eval_brdf_diffuse(in BRDFInfo brdf, in SurfaceInfo si, in vec3 wo) {
  float cos_theta_i = cos_theta(si.wi), 
        cos_theta_o = cos_theta(wo);

  if (cos_theta_i <= 0.f || cos_theta_o <= 0.f)
    return vec4(0.f);
    
  return brdf.r * M_PI_INV;
}

float pdf_brdf_diffuse(in BRDFInfo brdf, in SurfaceInfo si, in vec3 wo) {
  float cos_theta_i = cos_theta(si.wi), 
        cos_theta_o = cos_theta(wo);

  if (cos_theta_i <= 0.f || cos_theta_o <= 0.f)
    return 0.f;
  
  return square_to_cos_hemisphere_pdf(wo);
}

#endif // BRDF_DIFFUSE_GLSL_GUARD