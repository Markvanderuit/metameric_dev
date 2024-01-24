#ifndef BRDF_GLSL_GUARD
#define BRDF_GLSL_GUARD

// This header requires the following defines to point to SSBOs or shared memory
// to work around glsl's lack of ssbo argument passing
// #define brdf_buff_objc_info buff_objc_info.data
// #define brdf_buff_bary_info buff_weights.data1
// #define brdf_txtr_bary      b_bary_4f
// #define brdf_txtr_spec      b_spec_4f

#include <math.glsl>
#include <record.glsl>
#include <scene.glsl>
#include <surface.glsl>
#include <brdf/null.glsl>
#include <brdf/diffuse.glsl>

BRDFInfo get_brdf(in SurfaceInfo si, vec4 wvls) {
  BRDFInfo brdf;
  brdf.type = is_object(si) ? BRDFTypeDiffuse : BRDFTypeNull;

  if (brdf.type == BRDFTypeDiffuse) {
    init_brdf_diffuse(brdf, si, wvls);
  } else if (brdf.type == BRDFTypeNull) {
    init_brdf_null(brdf, si, wvls);
  } /* else if (...) {
    // ...
  } */

  return brdf;
}

BRDFSample sample_brdf(in BRDFInfo brdf, in vec2 sample_2d, in SurfaceInfo si) {
  if (brdf.type == BRDFTypeDiffuse) {
    return sample_brdf_diffuse(brdf, sample_2d, si);
  } else if (brdf.type == BRDFTypeNull) {
    return sample_brdf_null(brdf, sample_2d, si);
  } /* else if (...) {
    // ...
  } */
}

vec4 eval_brdf(in BRDFInfo brdf, in SurfaceInfo si, in vec3 wo) {
  if (brdf.type == BRDFTypeDiffuse) {
    return eval_brdf_diffuse(brdf, si, wo);
  } else if (brdf.type == BRDFTypeNull) {
    return eval_brdf_null(brdf, si, wo);
  } /* else if (...) {
    // ...
  } */
}

float pdf_brdf(in BRDFInfo brdf, in SurfaceInfo si, in vec3 wo) {
  if (brdf.type == BRDFTypeDiffuse) {
    return pdf_brdf_diffuse(brdf, si, wo);
  } else if (brdf.type == BRDFTypeNull) {
    return pdf_brdf_null(brdf, si, wo);
  } /* else if (...) {
    // ...
  } */
}

#endif // BRDF_GLSL_GUARD