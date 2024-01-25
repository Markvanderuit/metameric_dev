#ifndef BRDF_NULL_GLSL_GUARD
#define BRDF_NULL_GLSL_GUARD

void init_brdf_null(inout BRDFInfo brdf, in SurfaceInfo si, vec4 wvls) {
  brdf.r = vec4(1);
}

BRDFSample sample_brdf_null(in BRDFInfo brdf, in vec2 sample_2d, in SurfaceInfo si) {
  BRDFSample bs;

  bs.f        = vec4(1);
  bs.wo       = frame_to_world(si.sh, si.wi);
  bs.is_delta = true;
  bs.pdf      = 1.f;

  return bs;
}

vec4 eval_brdf_null(in BRDFInfo brdf, in SurfaceInfo si, in vec3 wo) {
  return vec4(0);
}

float pdf_brdf_null(in BRDFInfo brdf, in SurfaceInfo si, in vec3 wo) {
  return 0.f;
}

#endif // BRDF_NULL_GLSL_GUARD