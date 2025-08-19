#ifndef BRDF_NULL_GLSL_GUARD
#define BRDF_NULL_GLSL_GUARD

#include <render/record.glsl>

BRDFSample sample_brdf_null(in BRDF brdf, in vec3 sample_3d, in Interaction si, in vec4 wvls) {
  BRDFSample bs;

  bs.is_spectral = false;
  bs.is_delta = true;
  bs.wo       = -si.wi;
  bs.pdf      = 1.f;

  return bs;
}

vec4 eval_brdf_null(in BRDF brdf, in Interaction si, in vec3 wo, in vec4 wvls) {
  return vec4(0);
}

float pdf_brdf_null(in BRDF brdf, in Interaction si, in vec3 wo) {
  return 0.f;
}

#endif // BRDF_NULL_GLSL_GUARD