#ifndef BRDF_GLSL_GUARD
#define BRDF_GLSL_GUARD

#include <math.glsl>
#include <record.glsl>
#include <scene.glsl>
#include <surface.glsl>
#include <warp.glsl>

// This header requires the following defines to point to SSBOs or shared memory
// to work around glsl's lack of ssbo argument passing
// #define brdf_buff_objc_info buff_objc_info.data
// #define brdf_buff_bary_info buff_weights.data1
// #define brdf_txtr_bary      b_bary_4f
// #define brdf_txtr_spec      b_spec_4f

// Partial BRDF data, used to front-load some intermediary work
struct PreliminaryBRDF {
  int   index; // -1 if they differ, index otherwise
  vec3  tx;    // Barycentric texture coordinate
};

// Simple diffuse BRDF data whose albedo underlies a spectral material
struct BRDF {
  vec4 r; // Surface albedo for four wavelengths
};

PreliminaryBRDF get_surface_brdf_preliminary(in SurfaceInfo si) {
  PreliminaryBRDF brdf;

  // Load relevant info objects
  ObjectInfo  object_info = brdf_buff_objc_info[record_get_object(si.data)];
  AtlasLayout weight_info = brdf_buff_bary_info[record_get_object(si.data)];

  // Translate gbuffer uv to texture atlas coordinate for the barycentrics;
  // also handle single-color objects by sampling the center of their patch
  vec2 tx_si = object_info.is_albedo_sampled ? si.tx : vec2(0.5f);
  vec3 tx_uv = vec3(weight_info.uv0 + weight_info.uv1 * tx_si, weight_info.layer);

  brdf.index = -1;
  brdf.tx    = vec3(weight_info.uv0 + weight_info.uv1 * tx_si, weight_info.layer);

  ivec4 indices = ivec4(textureGather(brdf_txtr_bary, brdf.tx, 3));
  if (all(equal(indices, ivec4(indices[0])))) {
    brdf.index = indices[0];
  }

  return brdf;
}

BRDF get_surface_brdf(in PreliminaryBRDF pr, vec4 wavelength) {
  BRDF brdf;

  // Obtain spectral reflectance
  if (pr.index >= 0) {
    // Hot path; all element indices are the same, so use the one index for texture lookups

    // Sample barycentric weights
    vec4 bary = textureLod(brdf_txtr_bary, pr.tx, 0);
    bary.w    = 1.f - hsum(bary.xyz);

    // For each wavelength, sample and compute reflectance
    // Reflectance is dot product of barycentrics and reflectances
    for (uint i = 0; i < 4; ++i) {
      vec4 refl = texture(brdf_txtr_spec, vec2(wavelength[i], pr.index));
      brdf.r[i] = dot(bary, refl);
    } // for (uint i)
  } else {
    const ivec2 tx_offsets[4] = ivec2[4](ivec2(0, 0), ivec2(1, 0), ivec2(0, 1), ivec2(1, 1));

    // Cold path; element indices differ. Do costly interpolation manually :(

    // Scale up to full texture size
    vec3  tx       = pr.tx * vec3(textureSize(brdf_txtr_bary, 0).xy, 1) - vec3(0.5, 0.5, 0);
    ivec3 tx_floor = ivec3(tx);
    vec2  alpha    = mod(tx.xy, 1.f);
    
    mat4 r;
    for (uint i = 0; i < 4; ++i) {
      // Sample barycentric weights
      vec4 bary   = texelFetch(brdf_txtr_bary, tx_floor + ivec3(tx_offsets[i], 0), 0);
      uint elem_i = uint(bary.w);
      bary.w      = 1.f - hsum(bary.xyz);

      // For each wavelength, sample and compute reflectance
      // Reflectance is dot product of barycentrics and reflectances
      for (uint j = 0; j < 4; ++j) {
        vec4 refl = texture(brdf_txtr_spec, vec2(wavelength[j], elem_i));
        r[i][j] = dot(bary, refl);
      } // for (uint j)
    } // for (uint i)

    brdf.r = mix(mix(r[0], r[1], alpha.x), mix(r[2], r[3], alpha.x), alpha.y);
  }  

  return brdf;
}

BRDF get_surface_brdf(in SurfaceInfo si, vec4 wavelength) {
  BRDF brdf;
  PreliminaryBRDF pr = get_surface_brdf_preliminary(si);
  return get_surface_brdf(pr, wavelength);
}

BRDFSample sample_brdf(in BRDF brdf, in vec2 sample_2d, in SurfaceInfo si) {
  BRDFSample bs;

 /*  if (frame_cos_theta(si.wi) <= 0.f) {
    bs.pdf = 0.f;
    return bs;
  } */

  vec3 wo = square_to_cos_hemisphere(sample_2d);

  bs.f   = brdf.r;
  bs.pdf = square_to_cos_hemisphere_pdf(wo);
  bs.wo  = frame_to_world(si.sh, wo);

  return bs;
}

vec4 eval_brdf(in BRDF brdf, in SurfaceInfo si, in vec3 wo) {
  wo = frame_to_local(si.sh, wo);

  /* float cos_theta_i = frame_cos_theta(si.wi), 
        cos_theta_o = frame_cos_theta(wo);
  
  if (cos_theta_i <= 0.f || cos_theta_o <= 0.f)
    return vec4(0.f); */
    
  return brdf.r * M_PI_INV * abs(frame_cos_theta(wo));
}

float pdf_brdf(in BRDF brdf, in SurfaceInfo si, in vec3 wo) {
  wo = frame_to_local(si.sh, wo);

  /* float cos_theta_i = frame_cos_theta(si.wi), 
        cos_theta_o = frame_cos_theta(wo);

  if (cos_theta_i <= 0.f || cos_theta_o <= 0.f)
    return 0.f; */
  
  return square_to_cos_hemisphere_pdf(wo);
}

#endif // BRDF_GLSL_GUARD