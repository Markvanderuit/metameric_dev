#ifndef REFLECTANCE_GLSL_GUARD
#define REFLECTANCE_GLSL_GUARD

#ifdef  SCENE_DATA_REFLECTANCE
  // Constant-expression texel corners used for manual texture interpolation
  const vec2 tx_offsets[4] = { vec2(0, 0), vec2(1, 0), vec2(0, 1), vec2(1, 1) };
  
  // Translate object texture coordinates to coordinates suited for a texture atlas;
  // baked spectral texture coefficients live in an atlas in this implementation, 
  // so this step is necessary.
  vec3 tx_to_atlas(in TextureAtlasInfo atlas_info, in vec2 tx) {
    vec3 tx_atlas;
    tx_atlas = vec3(atlas_info.uv0 + atlas_info.uv1 * tx, atlas_info.layer);
    tx_atlas.xy *= scene_barycentric_data_size(); // Scale [0,1] to texture size
    tx_atlas.xy -= 0.5f;                          // Offset by half a pixel
    return tx_atlas;
  }

  vec4 scene_sample_reflectance(in uint object_i, in vec2 tx, in vec4 wvls) {
    // Load relevant info objects
    ObjectInfo       object_info = scene_object_info(object_i);
    TextureAtlasInfo atlas_info  = scene_reflectance_atlas_info(object_i);

    // Translate object uv to texture atlas uv for the baked spectral texture coefficients
    vec3 tx_atlas = tx_to_atlas(atlas_info, record_is_sampled(object_info.albedo_data) ? tx : vec2(0.5f));

    // Return value; reflectance for four wavelengths
    vec4 r = vec4(0);

    // Mix four texels appropriately, sampling each of the four wavelengths independently
    for (uint i = 0; i < 4; ++i) { // four texel corners
      // Texel mixing weight for a particular corner
      float w = hprod(mix(vec2(1) - fract(tx_atlas.xy), fract(tx_atlas.xy), tx_offsets[i]));

      // Load packed basis coefficients for a particular corner
      uvec4 cpack = scene_coefficients_data_fetch(ivec3(tx_atlas) + ivec3(tx_offsets[i], 0));

      // Iterate the bases
      for (uint k = 0; k < wavelength_bases; ++k) {
#ifdef SCENE_DATA_REFLECTANCE_BUCKETED
        // Extract k'th basis coefficient, multiply with presampled basis, 
        // and then multiply by texel mixing weight
        r += w                       // texel mixing weight
           * extract_basis_coeff(cpack, k) // next texture coefficient
           * scene_basis_func(k);    // next basis weight
#else // SCENE_DATA_REFLECTANCE_BUCKETED
        // Extract k'th coefficient, multiply by texel mixing weight
        float a = w * extract_basis_coeff(cpack, k);

        // Iterate 4 wavelengths and perform matrix product with basis
        for (uint j = 0; j < 4; ++j)
          r[j] += a * scene_basis_func(wvls[j], k);
#endif // SCENE_DATA_REFLECTANCE_BUCKETED
      } // for (uint k)
    } // for (uint i)

    // Should not be necessary, but just in case
    return clamp(r, 0, 1);
  }

  vec4 scene_sample_reflectance_stochastic(in uint object_i, in vec2 tx, in vec4 wvls, in vec2 sample_2d) {
    // Load relevant info objects
    ObjectInfo       object_info = scene_object_info(object_i);
    TextureAtlasInfo atlas_info  = scene_reflectance_atlas_info(object_i);

    // Translate object uv to texture atlas uv for the baked spectral texture coefficients
    vec3 tx_atlas = tx_to_atlas(atlas_info, record_is_sampled(object_info.albedo_data) ? tx : vec2(0.5f));

    // Sample a texel for stochastic mixing; the interpolation weight of that texel equals
    // the sampling density, so we can elimitate it and the density from further computation
    uint i = hsum(mix(uvec2(0), uvec2(1, 2), greaterThanEqual(sample_2d, vec2(1) - fract(tx_atlas.xy))));
    /* float w = hprod(mix(vec2(1) - fract(tx_atlas.xy), fract(tx_atlas.xy), tx_offsets[i])); */

    // Load packed basis coefficients for a particular corner
    uvec4 cpack = scene_coefficients_data_fetch(ivec3(tx_atlas) + ivec3(tx_offsets[i], 0));

    // Return value; reflectance for four wavelengths
    vec4 r = vec4(0);

    // Iterate the bases
    for (uint k = 0; k < wavelength_bases; ++k) {
#ifdef SCENE_DATA_REFLECTANCE_BUCKETED
      // Extract k'th basis coefficient, multiply with presampled basis
      r +=  extract_basis_coeff(cpack, k) * scene_basis_func(k);
#else // SCENE_DATA_REFLECTANCE_BUCKETED
      // Extract k'th coefficient, multiply by texel mixing weight
      float a = extract_basis_coeff(cpack, k);

      // Iterate 4 wavelengths and perform matrix product with basis
      for (uint j = 0; j < 4; ++j)
        r[j] += a * scene_basis_func(wvls[j], k);
#endif // SCENE_DATA_REFLECTANCE_BUCKETED
    } // for (uint k)

    // Clamping should not be necessary, but it can't hurt
    return clamp(r, 0, 1);
  }

#elif defined(SCENE_DATA_RGB) 

  vec4 scene_sample_reflectance(in uint object_i, in vec2 tx, in vec4 wvls /* ignored */) {
    // Load relevant info objects
    ObjectInfo object_info = scene_object_info(object_i);
    if (record_is_sampled(object_info.albedo_data)) {
      TextureInfo txtr  = scene_rgb_atlas_info(record_get_sampler_index(object_info.albedo_data));
      vec3 tx_uv = vec3(txtr.uv0 + txtr.uv1 * tx, txtr.layer);
      return vec4(scene_rgb_data_texture(tx_uv).xyz, 1); // Discard alpha for now
    } else {
      return vec4(record_get_direct_value(object_info.albedo_data), 1);
    }
  }

  vec4 scene_sample_reflectance_stochastic(in uint object_i, in vec2 tx, in vec4 wvls /* ignored */, in vec2 sample_2d /* ignored */) {
    // Load relevant info objects
    ObjectInfo object_info = scene_object_info(object_i);
    if (record_is_sampled(object_info.albedo_data)) {
      TextureInfo txtr  = scene_rgb_atlas_info(record_get_sampler_index(object_info.albedo_data));
      vec3 tx_uv = vec3(txtr.uv0 + txtr.uv1 * tx, txtr.layer);
      return vec4(scene_rgb_data_texture(tx_uv).xyz, 1); // Discard alpha for now
    } else {
      return vec4(record_get_direct_value(object_info.albedo_data), 1);
    }
  }

#else  // SCENE_DATA_REFLECTANCE
  vec4 scene_sample_reflectance(in uint object_i, in vec2 tx, in vec4 wvls) { return vec4(1); }
  vec4 scene_sample_reflectance_stochastic(in uint object_i, in vec2 tx, in vec4 wvls, in vec2 sample_2d) { return vec4(1); }
#endif // SCENE_DATA_REFLECTANCE
#endif // REFLECTANCE_GLSL_GUARD