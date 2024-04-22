#ifndef REFLECTANCE_GLSL_GUARD
#define REFLECTANCE_GLSL_GUARD

#ifdef SCENE_DATA_REFLECTANCE

// #define SCENE_REFLECTANCE_MESE
// #ifdef SCENE_REFLECTANCE_MESE
  #include <moments.glsl>

  // Helpers to convert wavelengths in this program's smaller wavelength range
  // to the correct phase warp in the full CIE range.
  const float warp_wavelength_min = 360.f;
  const float warp_wavelength_max = 830.f;
  const float warp_offs = (float(wavelength_min) - warp_wavelength_min) 
                        / (warp_wavelength_max - warp_wavelength_min);
  const float warp_mult = (float(wavelength_max - wavelength_min)) 
                        / (warp_wavelength_max - warp_wavelength_min);
  vec4 wvls_to_phase(in vec4 wvls) {
    for (uint i = 0; i < 4; ++i)
      wvls[i] = scene_phase_warp_data_texture(fma(wvls[i], warp_mult, warp_offs));
    return wvls;
  }

  // Cold path; element indices differ. Do costly interpolation manually :(
  const ivec2 reflectance_tx_offsets[4] = ivec2[4](ivec2(0, 0), ivec2(1, 0), ivec2(0, 1), ivec2(1, 1));

  vec4 scene_sample_reflectance_moments(in uint object_i, in vec2 tx, in vec4 wvls) {
    // Load relevant info objects
    ObjectInfo      object_info       = scene_object_info(object_i);
    BarycentricInfo barycentrics_info = scene_reflectance_barycentric_info(object_i);

    // Translate gbuffer uv to texture atlas coordinate for the barycentrics;
    // also handle single-color objects by sampling the center of their patch
    vec2 tx_si = object_info.is_albedo_sampled ? tx : vec2(0.5f);
    vec3 tx_uv = vec3(barycentrics_info.uv0 + barycentrics_info.uv1 * tx_si, barycentrics_info.layer);

    // Scale up to full texture size
    vec3 tx_3d = tx_uv * vec3(scene_barycentric_data_size(), 1) - vec3(0.5, 0.5, 0);
    vec2 alpha = mod(tx_3d.xy, 1.f);

    vec4 r = vec4(0); 
    vec4 p = wvls_to_phase(wvls);

    // Output reflectance, manual mixture of four texels
    for (uint i = 0; i < 4; ++i) {
      // Sample packed moment coefficients
      uvec4 v = scene_coefficients_data_fetch(ivec3(tx_3d) + ivec3(reflectance_tx_offsets[i], 0));

      // Generate reflectance and add with correct mixture for each texel
      r += hprod(mix(vec2(1) - alpha, alpha, vec2(reflectance_tx_offsets[i]))) 
         * moments_to_reflectance(p, unpack_half_8x16(v));
    } // for (uint i)

    return mix(r, vec4(0), isnan(r)); // Catch all-black glitching out during mixing
  }
  
  vec4 scene_sample_reflectance_bases(in uint object_i, in vec2 tx, in vec4 wvls) {
    // Load relevant info objects
    ObjectInfo      object_info       = scene_object_info(object_i);
    BarycentricInfo barycentrics_info = scene_reflectance_barycentric_info(object_i);

    // Translate gbuffer uv to texture atlas coordinate for the barycentrics;
    // also handle single-color objects by sampling the center of their patch
    vec2 tx_si = object_info.is_albedo_sampled ? tx : vec2(0.5f);
    vec3 tx_uv = vec3(barycentrics_info.uv0 + barycentrics_info.uv1 * tx_si, barycentrics_info.layer);

    // Scale up to full texture size
    vec3 tx_3d = tx_uv * vec3(scene_barycentric_data_size(), 1) - vec3(0.5, 0.5, 0);
    vec2 alpha = mod(tx_3d.xy, 1.f);

    // Return value; reflectance for four wavelengths
    vec4 r = vec4(0); 

    // Mix four texels appropriately, sampling each of four wavelengths independently
    for (uint i = 0; i < 4; ++i) { // four texel corners
      float[wavelength_bases] c = unpack_snorm_12(scene_coefficients_data_fetch(ivec3(tx_3d) + ivec3(reflectance_tx_offsets[i], 0)));
      float w = hprod(mix(vec2(1) - alpha, alpha, vec2(reflectance_tx_offsets[i])));
      for (uint j = 0; j < 4; ++j) {                // four wavelengths
        for (uint k = 0; k < wavelength_bases; ++k) // n bases
          r[j] += w * c[k] * scene_basis_func(wvls[j], k);
      }
    }

    return clamp(r, 0, 1);
  }

// #else // SCENE_REFLECTANCE_MESE

  // Helper to perform spectrum interpolation given all relevant information;
  // - what tetrahedron of spectra to access
  // - what barycentrics to use
  vec4 detail_mix_reflectances(in vec4 wvls, in vec4 bary, in uint index) {
    vec4 r;
    for (uint i = 0; i < 4; ++i)
      r[i] = dot(bary, scene_spectral_data_texture(vec2(wvls[i], index)));
    return r;
  }

  vec4 scene_sample_reflectance_barycentrics(in uint object_i, in vec2 tx, in vec4 wvls) {
    // Load relevant info objects
    ObjectInfo      object_info       = scene_object_info(object_i);
    BarycentricInfo barycentrics_info = scene_reflectance_barycentric_info(object_i);

    // Translate gbuffer uv to texture atlas coordinate for the barycentrics;
    // also handle single-color objects by sampling the center of their patch
    vec2 tx_si = object_info.is_albedo_sampled ? tx : vec2(0.5f);
    vec3 tx_uv = vec3(barycentrics_info.uv0 + barycentrics_info.uv1 * tx_si, barycentrics_info.layer);

    // Fill atlas texture coordinates, and spectral index
    vec4 r = vec4(0); 
    if (is_all_equal(ivec4(scene_barycentric_data_gather_w(tx_uv)))) { // Hot path; all element indices are the same, so use the one index for texture lookups
      // Sample barycentric weights
      vec4 bary   = scene_barycentric_data_texture(tx_uv);
      uint elem_i = uint(bary.w);
      bary.w      = 1.f - hsum(bary.xyz);

      // For each wvls, sample and compute reflectance
      // Reflectance is dot product of barycentrics and reflectances
      r = detail_mix_reflectances(wvls, bary, elem_i);
    } else {  
      // Scale up to full texture size
      vec3 tx    = tx_uv * vec3(scene_barycentric_data_size(), 1) - vec3(0.5, 0.5, 0);
      vec2 alpha = mod(tx.xy, 1.f);

      // Output reflectance, manual mixture of four texels
      for (uint i = 0; i < 4; ++i) {
        // Sample barycentric weights
        vec4 bary   = scene_barycentric_data_fetch(ivec3(tx) + ivec3(reflectance_tx_offsets[i], 0));
        uint elem_i = uint(bary.w);
        bary.w      = 1.f - hsum(bary.xyz);

        // For each of four wvls, sample and compute reflectance
        // Reflectance is dot product of barycentrics and reflectances
        r += hprod(mix(vec2(1) - alpha, alpha, vec2(reflectance_tx_offsets[i]))) 
           * detail_mix_reflectances(wvls, bary, elem_i);
      } // for (uint i)
    }

    return r;
  }
  
  // Forward to whatever sampler we're experimenting with today
  vec4 scene_sample_reflectance(in uint object_i, in vec2 tx, in vec4 wvls) {
    return scene_sample_reflectance_barycentrics(object_i, tx, wvls);
    // return scene_sample_reflectance_bases(object_i, tx, wvls);
    // return scene_sample_reflectance_moments(object_i, tx, wvls);
  }

// #endif // SCENE_REFLECTANCE_MESE
#else  // SCENE_DATA_REFLECTANCE
  vec4 scene_sample_reflectance(in uint object_i, in vec2 tx, in vec4 wvls) { return vec4(1); }
#endif // SCENE_DATA_REFLECTANCE
#endif // REFLECTANCE_GLSL_GUARD