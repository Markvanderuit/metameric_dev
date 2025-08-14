#ifndef TEXTURE_GLSL_GUARD
#define TEXTURE_GLSL_GUARD

#include <render/warp.glsl>

// Constant-expression texel corners used for manual texture interpolation
const vec2 tx_offsets[4] = { vec2(0, 0), vec2(1, 0), vec2(0, 1), vec2(1, 1) };

// Transform texture coordinates in object's definition to texture coordinates in
// texture atlas' definition, given a specific patch and the total atlas' 2d size
vec3 tx2_to_atlas_tx3(in AtlasInfo atlas_patch, in vec2 tx2, in vec2 atlas_size) {
  // Translate to texture atlas patch
  vec3 tx3 = vec3(atlas_patch.uv0 + atlas_patch.uv1 * tx2, atlas_patch.layer);
  tx3.xy *= atlas_size; // Scale [0,1] to texture size
  tx3.xy -= 0.5f;       // Offset by half a pixel
  
  // Clamp to texture atlas patch
  tx3.xy = clamp(tx3.xy, vec2(atlas_patch.offs), vec2(atlas_patch.offs + atlas_patch.size - 1));

  return tx3;
}

// Translate object texture coordinates to coordinates suited for a texture atlas;
// baked spectral texture coefficients live in an atlas in this implementation, 
// so this step is necessary.
vec3 si_to_object_coef_atlas_tx(in SurfaceInfo si) {
  // Obtain uv coordinates, or set to 0.5 if the albedo value is specified
  ObjectInfo object_info = scene_object_info(record_get_object(si.data));
  vec2 tx2 = record_is_sampled(object_info.albedo_data) ? si.tx : vec2(0.5f);

  // Return coordinates in texture atlas
  AtlasInfo atlas_info = scene_texture_object_coef_info(record_get_object(si.data));
  return tx2_to_atlas_tx3(atlas_info, tx2, scene_texture_object_coef_size());
}

// Same as above, different atlas
vec3 si_to_object_brdf_atlas_tx(in SurfaceInfo si) {
  // Obtain uv coordinates, or set to 0.5 if the albedo value is specified
  ObjectInfo object_info = scene_object_info(record_get_object(si.data));
  vec2 tx2 = record_is_sampled(object_info.albedo_data) ? si.tx : vec2(0.5f);

  // Return coordinates in texture atlas
  AtlasInfo atlas_info = scene_texture_object_brdf_info(record_get_object(si.data));
  return tx2_to_atlas_tx3(atlas_info, tx2, scene_texture_object_brdf_size());
}

// Same as above, but for illuminant data
vec3 tx_to_emitter_coef_atlas_tx(in uint emitter_i, in vec2 tx2) {
  AtlasInfo atlas_info = scene_texture_emitter_coef_info(emitter_i);
  return tx2_to_atlas_tx3(atlas_info, tx2, scene_texture_emitter_coef_size());
}

// Sample four-wavelength surface reflectances using stochastic sampling; we avoid
// doing four texel fetches + unpacking, and instead do simple stochastic filtering
vec4 texture_reflectance(in SurfaceInfo si, in vec4 wvls, in vec2 sample_2d) {
  // Translate surface uv data to texture atlas uv
  vec3 tx = si_to_object_coef_atlas_tx(si);
  
  // Sample a texel offset for stochastic mixing; the interpolation weight of that texel equals
  // the sampling density, so we can elimitate both it and the density from further computation
  uint i = hsum(mix(uvec2(0), uvec2(1, 2), greaterThanEqual(sample_2d, vec2(1) - fract(tx.xy))));

  // Load packed basis coefficients for a particular corner
  uvec4 pack = scene_texture_object_coef_fetch(ivec3(tx) + ivec3(tx_offsets[i], 0));

  // Return value; reflectance for four wavelengths
  vec4 r = vec4(0);

  // Iterate the bases
  for (uint k = 0; k < wavelength_bases; ++k) {
    // Extract k'th coefficient, multiply by texel mixing weight
    float a = extract_basis_coeff(pack, k);

    // Iterate 4 wavelengths and perform matrix product with basis
    for (uint j = 0; j < 4; ++j)
      r[j] += a * scene_texture_basis_sample(wvls[j], k);
  } // for (uint k)

  return r;
}

vec2 texture_brdf(in SurfaceInfo si, in vec2 sample_2d) {
  // Translate surface uv data to texture atlas uv
  vec3 tx = si_to_object_brdf_atlas_tx(si);

  // Sample a texel offset for stochastic mixing; the interpolation weight of that texel equals
  // the sampling density, so we can elimitate both it and the density from further computation
  uint i = hsum(mix(uvec2(0), uvec2(1, 2), greaterThanEqual(sample_2d, vec2(1) - fract(tx.xy))));
  
  // Load packed brdf data for a particular corner
  uint pack = scene_texture_object_brdf_fetch(ivec3(tx) + ivec3(tx_offsets[i], 0));

  return unpackHalf2x16(pack);
}

vec4 texture_illuminant(in uint emitter_i, in vec2 tx2, in vec4 wvls, in vec2 sample_2d) {
  // Translate provided uv data to texture atlas uv
  vec3 tx = tx_to_emitter_coef_atlas_tx(emitter_i, tx2);

  // Sample a texel offset for stochastic mixing; the interpolation weight of that texel equals
  // the sampling density, so we can elimitate both it and the density from further computation
  uint i = hsum(mix(uvec2(0), uvec2(1, 2), greaterThanEqual(sample_2d, vec2(1) - fract(tx.xy))));

  // Load packed basis coefficients for a particular corner
  uvec4 pack = scene_texture_emitter_coef_fetch(ivec3(tx) + ivec3(tx_offsets[i], 0));

  // Return value; reflectance for four wavelengths
  vec4 e = vec4(0);

  // Iterate the bases
  for (uint k = 0; k < wavelength_bases; ++k) {
    // Extract k'th coefficient, multiply by texel mixing weight
    float a = extract_basis_coeff(pack, k);

    // Iterate 4 wavelengths and perform matrix product with basis
    for (uint j = 0; j < 4; ++j)
      e[j] += a * scene_texture_basis_sample(wvls[j], k);
  } // for (uint k)

  return e * scene_texture_emitter_scle_fetch(ivec3(tx) + ivec3(tx_offsets[i], 0));
}

vec4 texture_illuminant(in SurfaceInfo si, in vec4 wvls, in vec2 sample_2d) {
  return texture_illuminant(record_get_emitter(si.data), si.tx, wvls, sample_2d);
}

#endif // TEXTURE_GLSL_GUARD