#ifndef BRDF_DIFFUSE_GLSL_GUARD
#define BRDF_DIFFUSE_GLSL_GUARD

#include <render/warp.glsl>

// Partial BRDFInfo data, used to front-load some intermediary work
// for the spectral reflectance data
struct PreliminaryDiffuseBRDF {
  int   index; // -1 if they differ, index otherwise
  vec3  tx;    // Barycentric texture coordinate
};

PreliminaryDiffuseBRDF init_brdf_diffuse_preliminary(in SurfaceInfo si) {
  PreliminaryDiffuseBRDF pr;

  // Load relevant info objects
  ObjectInfo      object_info       = scene_object_info(record_get_object(si.data));
  BarycentricInfo barycentrics_info = scene_reflectance_barycentric_info(record_get_object(si.data));

  // Translate gbuffer uv to texture atlas coordinate for the barycentrics;
  // also handle single-color objects by sampling the center of their patch
  vec2 tx_si = object_info.is_albedo_sampled ? si.tx : vec2(0.5f);
  vec3 tx_uv = vec3(barycentrics_info.uv0 + barycentrics_info.uv1 * tx_si, barycentrics_info.layer);

  pr.index = -1;
  pr.tx    = vec3(barycentrics_info.uv0 + barycentrics_info.uv1 * tx_si, barycentrics_info.layer);

  ivec4 indices = ivec4(textureGather(scene_reflectance_barycentrics(), pr.tx, 3));
  if (all(equal(indices, ivec4(indices[0])))) {
    pr.index = indices[0];
  }

  return pr;
}

void init_brdf_diffuse(inout BRDFInfo brdf, in SurfaceInfo si, vec4 wvls) {
  PreliminaryDiffuseBRDF pr = init_brdf_diffuse_preliminary(si);
  
#ifndef ENABLE_PARTIAL_PATH_TRACKING
  // Obtain spectral reflectance
  if (pr.index >= 0) {
    // Hot path; all element indices are the same, so use the one index for texture lookups

    // Sample barycentric weights
    vec4 bary = textureLod(scene_reflectance_barycentrics(), pr.tx, 0);
    bary.w    = 1.f - hsum(bary.xyz);

    // For each wvls, sample and compute reflectance
    // Reflectance is dot product of barycentrics and reflectances
    for (uint i = 0; i < 4; ++i) {
      vec4 refl = texture(scene_reflectance_spectra(), vec2(wvls[i], pr.index));
      brdf.r[i] = dot(bary, refl);
    } // for (uint i)
  } else {
    const ivec2 tx_offsets[4] = ivec2[4](ivec2(0, 0), ivec2(1, 0), ivec2(0, 1), ivec2(1, 1));

    // Cold path; element indices differ. Do costly interpolation manually :(

    // Scale up to full texture size
    vec3  tx       = pr.tx * vec3(textureSize(scene_reflectance_barycentrics(), 0).xy, 1) - vec3(0.5, 0.5, 0);
    ivec3 tx_floor = ivec3(tx);
    vec2  alpha    = mod(tx.xy, 1.f);
    
    mat4 r;
    for (uint i = 0; i < 4; ++i) {
      // Sample barycentric weights
      vec4 bary   = texelFetch(scene_reflectance_barycentrics(), tx_floor + ivec3(tx_offsets[i], 0), 0);
      uint elem_i = uint(bary.w);
      bary.w      = 1.f - hsum(bary.xyz);

      // For each wvls, sample and compute reflectance
      // Reflectance is dot product of barycentrics and reflectances
      for (uint j = 0; j < 4; ++j) {
        vec4 refl = texture(scene_reflectance_spectra(), vec2(wvls[j], elem_i));
        r[i][j] = dot(bary, refl);
      } // for (uint j)
    } // for (uint i)

    brdf.r = mix(mix(r[0], r[1], alpha.x), mix(r[2], r[3], alpha.x), alpha.y);
  }
#else
  brdf.r = vec4(1);
#endif
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