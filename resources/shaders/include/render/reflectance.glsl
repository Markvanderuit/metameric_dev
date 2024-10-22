#ifndef REFLECTANCE_GLSL_GUARD
#define REFLECTANCE_GLSL_GUARD
#ifdef  SCENE_DATA_REFLECTANCE
  // If element indices differ. Do costly interpolation manually :(
  const ivec2 reflectance_tx_offsets[4] = ivec2[4](ivec2(0, 0), ivec2(1, 0), ivec2(0, 1), ivec2(1, 1));
  
  // Helper to load and unpack basis coefficients
  float[wavelength_bases] scene_sample_reflectance_coeffs(in ivec3 px) {
    return unpack_bases(scene_coefficients_data_fetch(px));
  }
  float[wavelength_bases] scene_sample_reflectance_coeffs(in vec3 px) {
    return unpack_bases(scene_coefficients_data_texture(px));
  }
  
  vec4 scene_sample_reflectance(in uint object_i, in vec2 tx, in vec4 wvls) {
    // Load relevant info objects
    ObjectInfo      object_info = scene_object_info(object_i);
    BarycentricInfo atlas_info  = scene_reflectance_atlas_info(object_i);

    // Translate gbuffer uv to texture atlas coordinate for the barycentrics;
    // also handle single-color objects by sampling the center of their patch
    vec3 tx_3d = vec3(atlas_info.uv0 + atlas_info.uv1 * (object_info.is_albedo_sampled ? tx : vec2(0.5f)), atlas_info.layer) 
               * vec3(scene_barycentric_data_size(), 1) - vec3(0.5, 0.5, 0);
    vec2 alpha = mod(tx_3d.xy, 1.f);

    // Return value; reflectance for four wavelengths
    vec4 r = vec4(0); 

    /* // Mix four texels appropriately, sampling each of four wavelengths independently
    for (uint i = 0; i < 4; ++i) { // four texel corners
      float[wavelength_bases] c = scene_sample_reflectance_coeffs(ivec3(tx_3d) + ivec3(reflectance_tx_offsets[i], 0));
      float w = hprod(mix(vec2(1) - alpha, alpha, vec2(reflectance_tx_offsets[i])));
      for (uint j = 0; j < 4; ++j) {                // four wavelengths
        for (uint k = 0; k < wavelength_bases; ++k) // n bases
          r[j] += w * c[k] * scene_basis_func(wvls[j], k);
      }
    } */

    // Mix four texels appropriately, sampling each of the four wavelengths independently
    for (uint i = 0; i < 4; ++i) { // four texel corners
      // Load packed basis coefficients
      uvec4 cpack = scene_coefficients_data_fetch(ivec3(tx_3d) + ivec3(reflectance_tx_offsets[i], 0));

      // Texel mixing weights
      float w = hprod(mix(vec2(1) - alpha, alpha, vec2(reflectance_tx_offsets[i])));

      // Iterate n bases
      for (uint k = 0; k < wavelength_bases; ++k) {
        // Extract k'th basis coefficient, multiply by texel mixing weight
        float a = w * extract_bases(cpack, k);
        
        // Iterate 4 wavelengths and perform matrix product with basis
        for (uint j = 0; j < 4; ++j) {
          #ifdef TEMP_BASIS_AVAILABLE
          r[j] += a * s_bucket_basis[bucket_id][k][j];
          #else
          r[j] += a * scene_basis_func(wvls[j], k);
          #endif
        }
      }
    }

    // Should not be necessary, but just in case
    return clamp(r, 0, 1);
  }

#elif defined(SCENE_DATA_RGB) 

  vec4 scene_sample_reflectance(in uint object_i, in vec2 tx, in vec4 wvls /* ignored */) {
    // Load relevant info objects
    ObjectInfo  object_info = scene_object_info(object_i);
    if (object_info.is_albedo_sampled) {
      TextureInfo txtr  = scene_rgb_atlas_info(object_info.albedo_i);
      // tx.xy = vec2(1) - vec2(tx.y, tx.x);
      vec3 tx_uv = vec3(txtr.uv0 + txtr.uv1 * tx, txtr.layer);
      return vec4(scene_rgb_data_texture(tx_uv).xyz, 1); // Discard alpha for now
    } else {
      return vec4(object_info.albedo_v, 1);
    }
  }

#else  // SCENE_DATA_REFLECTANCE
  vec4 scene_sample_reflectance(in uint object_i, in vec2 tx, in vec4 wvls) { return vec4(1); }
#endif // SCENE_DATA_REFLECTANCE
#endif // REFLECTANCE_GLSL_GUARD