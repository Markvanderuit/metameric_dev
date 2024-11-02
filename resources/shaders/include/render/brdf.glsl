#ifndef BRDF_GLSL_GUARD
#define BRDF_GLSL_GUARD

#include <render/sample.glsl>
#include <render/surface.glsl>
#include <render/texture.glsl>
#include <render/brdf/null.glsl>
#include <render/brdf/diffuse.glsl>
#include <render/brdf/principled.glsl>

BRDFInfo get_brdf(in SurfaceInfo si, vec4 wvls, in vec2 sample_2d) {
  BRDFInfo brdf;

  if (is_object(si)) {
    ObjectInfo object_info = scene_object_info(record_get_object(si.data));
    brdf.type = object_info.brdf_type;
  } else {
    brdf.type = BRDFTypeNull;
  }

  if (brdf.type == BRDFTypeDiffuse) {
    init_brdf_diffuse(brdf, si, wvls, sample_2d);
  } else if (brdf.type == BRDFTypeNull) {
    init_brdf_null(brdf, si, wvls);
  } else if (brdf.type == BRDFTypePrincipled) {
    init_brdf_principled(brdf, si, wvls, sample_2d);
  } /* else if (...) {
    // ...
  } */

  return brdf;
}

BRDFSample sample_brdf(in BRDFInfo brdf, in vec3 sample_3d, in SurfaceInfo si) {
  if (brdf.type == BRDFTypeDiffuse) {
    return sample_brdf_diffuse(brdf, sample_3d, si);
  } else if (brdf.type == BRDFTypeNull) {
    return sample_brdf_null(brdf, sample_3d, si);
  } else if (brdf.type == BRDFTypePrincipled) {
    return sample_brdf_principled(brdf, sample_3d, si);
  } /* else if (...) {
    // ...
  } */
}

vec4 eval_brdf(in BRDFInfo brdf, in SurfaceInfo si, in vec3 wo) {
  if (brdf.type == BRDFTypeDiffuse) {
    return eval_brdf_diffuse(brdf, si, wo);
  } else if (brdf.type == BRDFTypeNull) {
    return eval_brdf_null(brdf, si, wo);
  } else if (brdf.type == BRDFTypePrincipled) {
    return eval_brdf_principled(brdf, si, wo);
  } /* else if (...) {
    // ...
  } */
}

float pdf_brdf(in BRDFInfo brdf, in SurfaceInfo si, in vec3 wo) {
  if (brdf.type == BRDFTypeDiffuse) {
    return pdf_brdf_diffuse(brdf, si, wo);
  } else if (brdf.type == BRDFTypeNull) {
    return pdf_brdf_null(brdf, si, wo);
  } else if (brdf.type == BRDFTypePrincipled) {
    return pdf_brdf_principled(brdf, si, wo);
  } /* else if (...) {
    // ...
  } */
}

#endif // BRDF_GLSL_GUARD