#ifndef BRDF_GLSL_GUARD
#define BRDF_GLSL_GUARD

#include <render/sample.glsl>
#include <render/interaction.glsl>
#include <render/texture.glsl>
#include <render/brdf/null.glsl>
#include <render/brdf/diffuse.glsl>
#include <render/brdf/microfacet.glsl>
#include <render/brdf/dielectric.glsl>

// Helper to fill or precompute specific brdf data
// for some brdf types 
void detail_fill_brdf_data(inout BRDF brdf, in Object object, in vec4 wvls, in vec2 data) {
  if (brdf.type == BRDFTypeMicrofacet) {
    get_microfacet_alpha(brdf)    = max(1e-3, data.x * data.x);
    get_microfacet_metallic(brdf) = data.y;
    get_microfacet_eta(brdf)      = unpackHalf2x16(object.eta_data).x;
  } else if (brdf.type == BRDFTypeDielectric) {
    get_dielectric_absorption(brdf) = object.absorption;

    // x is min, y is max
    vec2 eta_minmax = unpackHalf2x16(object.eta_data);
    if (eta_minmax.x == eta_minmax.y || eta_minmax.x > eta_minmax.y) {
      // Effectively disables _brdf_eta_dispersive(...) in case configuration isn't spectral
      get_dielectric_eta(brdf) = eta_minmax.x;
      get_dielectric_dispersive(brdf) = 0;
    } else {
      // Compute cauchy coefficients b and c, then compute actual wavelength-dependent eta
      float lambda_min_2 = sdot(wavelength_min), lambda_max_2 = sdot(wavelength_max);
      float cauchy_b = (lambda_min_2 * eta_minmax.y - lambda_max_2 * eta_minmax.x) / (lambda_min_2 - lambda_max_2);
      float cauchy_c = lambda_min_2 * (eta_minmax.y - cauchy_b);
      get_dielectric_eta(brdf) = cauchy_b + cauchy_c / sdot(sample_to_wavelength(wvls.x));
      get_dielectric_dispersive(brdf) = 1;
    }
  } else {
    /* init_brdf_null(brdf, wvls); */
  }
}

BRDF get_brdf(inout Interaction si, vec4 wvls, in vec2 sample_2d) {
  BRDF brdf;
  
  if (is_object(si)) {
    // Query brdf type
    Object object = scene_object_info(record_get_object(si.data));
    brdf.type = object.brdf_type;

    // Query reflectance texture data
    brdf.r = texture_reflectance(si, wvls, sample_2d);

    // Unpack other brdf data
    vec4 data = texture_brdf(si, sample_2d);
    detail_fill_brdf_data(brdf, object, wvls, data.xy);

    // Unpack normalmap data;
    // Now that we've queried the underlying textures, we can adjust the 
    // local shading frame. This wasn't in use before this point.
    vec3 nm = unpack_normal_octahedral(data.zw);
    si.n = to_world(si, nm);
  } else {
    brdf.type = BRDFTypeNull;
  }

  return brdf;
}

BRDFSample sample_brdf(in BRDF brdf, in vec3 sample_3d, in Interaction si, in vec4 wvls) {
  if (brdf.type == BRDFTypeDiffuse) {
    return sample_brdf_diffuse(brdf, sample_3d, si, wvls);
  } else if (brdf.type == BRDFTypeNull) {
    return sample_brdf_null(brdf, sample_3d, si, wvls);
  } else if (brdf.type == BRDFTypeMicrofacet) {
    return sample_brdf_microfacet(brdf, sample_3d, si, wvls);
  } else if (brdf.type == BRDFTypeDielectric) {
    return sample_brdf_dielectric(brdf, sample_3d, si, wvls);
  } /* else if (...) {
    // ...
  } */
}

vec4 eval_brdf(in BRDF brdf, in Interaction si, in vec3 wo, in vec4 wvls) {
  if (brdf.type == BRDFTypeDiffuse) {
    return eval_brdf_diffuse(brdf, si, wo, wvls);
  } else if (brdf.type == BRDFTypeNull) {
    return eval_brdf_null(brdf, si, wo, wvls);
  } else if (brdf.type == BRDFTypeMicrofacet) {
    return eval_brdf_microfacet(brdf, si, wo, wvls);
  } else if (brdf.type == BRDFTypeDielectric) {
    return eval_brdf_dielectric(brdf, si, wo, wvls);
  } /* else if (...) {
    // ...
  } */
}

float pdf_brdf(in BRDF brdf, in Interaction si, in vec3 wo, in vec4 wvls) {
  if (brdf.type == BRDFTypeDiffuse) {
    return pdf_brdf_diffuse(brdf, si, wo);
  } else if (brdf.type == BRDFTypeNull) {
    return pdf_brdf_null(brdf, si, wo);
  } else if (brdf.type == BRDFTypeMicrofacet) {
    return pdf_brdf_microfacet(brdf, si, wo);
  } else if (brdf.type == BRDFTypeDielectric) {
    return pdf_brdf_dielectric(brdf, si, wo, wvls);
  } /* else if (...) {
    // ...
  } */
}

#endif // BRDF_GLSL_GUARD