#ifndef BRDF_GLSL_GUARD
#define BRDF_GLSL_GUARD

#include <render/sample.glsl>
#include <render/interaction.glsl>
#include <render/texture.glsl>
#include <render/brdf/null.glsl>
#include <render/brdf/diffuse.glsl>
#include <render/brdf/microfacet.glsl>
#include <render/brdf/dielectric.glsl>

BRDF get_brdf(inout Interaction si, vec4 wvls, in vec2 sample_2d) {
  BRDF brdf;
  
  if (is_object(si)) {
    // Query brdf type
    Object object = scene_object_info(record_get_object(si.data));
    brdf.type = object_brdf_type(object);

    // Query reflectance texture data
    brdf.r = texture_reflectance(si, wvls, sample_2d);

    // Read data pack from brdf parameter bake
    uvec4 data = floatBitsToUint(texture_brdf(si, sample_2d));

    // Unpack and assign brdf data
    brdf.metallic     = unpack_unorm_10((data[0]      ) & 0x03FF);             // 10b unorm
    brdf.alpha        = unpack_unorm_10((data[0] >> 10) & 0x03FF);             // 10b unorm
    brdf.transmission = unpack_unorm_10((data[0] >> 20) & 0x03FF);             // 10b unorm
    float eta_min     = float(((data[1]     ) & 0x00FFu)) / 255.f * 3.f + 1.f; // 8b unorm
    float eta_max     = float(((data[1] >> 8) & 0x00FFu)) / 255.f * 3.f + 1.f; // 8b unorm
    brdf.absorption   = unpackHalf2x16(((data[1] >> 16) & 0xFFFFu)).x;         // 16b half

    // Alpha is clamped to avoid specularities, and squared for perceptual niceness
    brdf.alpha = max(1e-3, brdf.alpha * brdf.alpha);

    // Compute cauchy coefficients b and c, then compute actual wavelength-dependent eta
    if (eta_max > eta_min) {
      float lambda_min_2 = sdot(wavelength_min), lambda_max_2 = sdot(wavelength_max);
      float cauchy_b = (lambda_min_2 * eta_max - lambda_max_2 * eta_min) / (lambda_min_2 - lambda_max_2);
      float cauchy_c = lambda_min_2 * (eta_max - cauchy_b);
      brdf.eta         = cauchy_b + cauchy_c / sdot(sample_to_wavelength(wvls.x));
      brdf.is_spectral = true;
    } else {
      brdf.eta         = eta_min;
      brdf.is_spectral = false;
    }

    // Unpack normalmap data;
    // Now that we've queried the underlying textures, we can adjust the 
    // local shading frame. This wasn't in use before this point.
    vec3 nm = unpack_normal_octahedral(uintBitsToFloat(data.zw));
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