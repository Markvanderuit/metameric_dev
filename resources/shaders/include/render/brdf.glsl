#ifndef BRDF_GLSL_GUARD
#define BRDF_GLSL_GUARD

#include <render/reflectance.glsl>
#include <render/surface.glsl>
#include <render/brdf/null.glsl>
#include <render/brdf/diffuse.glsl>
#include <render/brdf/mirror.glsl>
// #include <render/brdf/pbr.glsl>

BRDFInfo get_brdf(in SurfaceInfo si, vec4 wvls) {
  BRDFInfo brdf;

  if (is_object(si)) {
    ObjectInfo object_info = scene_object_info(record_get_object(si.data));
    brdf.type = object_info.brdf_type;
  } else {
    brdf.type = BRDFTypeNull;
  }

  if (brdf.type == BRDFTypeDiffuse) {
    init_brdf_diffuse(brdf, si, wvls);
  } else if (brdf.type == BRDFTypeNull) {
    init_brdf_null(brdf, si, wvls);
  } else if (brdf.type == BRDFTypeMirror) {
    init_brdf_mirror(brdf, si, wvls);
  } /* else if (brdf.type == BRDFTypePBR) {
    init_brdf_pbr(brdf, si, wvls);
  } */ /* else if (...) {
    // ...
  } */

  return brdf;
}

BRDFSample sample_brdf(in BRDFInfo brdf, in vec2 sample_2d, in SurfaceInfo si) {
  if (brdf.type == BRDFTypeDiffuse) {
    return sample_brdf_diffuse(brdf, sample_2d, si);
  } else if (brdf.type == BRDFTypeNull) {
    return sample_brdf_null(brdf, sample_2d, si);
  } else if (brdf.type == BRDFTypeMirror) {
    return sample_brdf_mirror(brdf, sample_2d, si);
  } /* else if (brdf.type == BRDFTypePBR) {
    return sample_brdf_pbr(brdf, sample_2d, si);
  } */ /* else if (...) {
    // ...
  } */
}

vec4 eval_brdf(in BRDFInfo brdf, in SurfaceInfo si, in vec3 wo) {
  if (brdf.type == BRDFTypeDiffuse) {
    return eval_brdf_diffuse(brdf, si, wo);
  } else if (brdf.type == BRDFTypeNull) {
    return eval_brdf_null(brdf, si, wo);
  } else if (brdf.type == BRDFTypeMirror) {
    return eval_brdf_mirror(brdf, si, wo);
  } /* else if (brdf.type == BRDFTypePBR) {
    return eval_brdf_pbr(brdf, si, wo);
  } */ /* else if (...) {
    // ...
  } */
}

float pdf_brdf(in BRDFInfo brdf, in SurfaceInfo si, in vec3 wo) {
  if (brdf.type == BRDFTypeDiffuse) {
    return pdf_brdf_diffuse(brdf, si, wo);
  } else if (brdf.type == BRDFTypeNull) {
    return pdf_brdf_null(brdf, si, wo);
  }  else if (brdf.type == BRDFTypeMirror) {
    return pdf_brdf_mirror(brdf, si, wo);
  } /* else if (brdf.type == BRDFTypePBR) {
    return pdf_brdf_pbr(brdf, si, wo);
  } */ /* else if (...) {
    // ...
  } */
}

#endif // BRDF_GLSL_GUARD