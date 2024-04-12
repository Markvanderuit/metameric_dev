#ifndef REFLECTANCE_GLSL_GUARD
#define REFLECTANCE_GLSL_GUARD

#ifdef SCENE_DATA_REFLECTANCE
  #include <moments.glsl>

  // Cold path; element indices differ. Do costly interpolation manually :(
  const ivec2 reflectance_tx_offsets[4] = ivec2[4](ivec2(0, 0), ivec2(1, 0), ivec2(0, 1), ivec2(1, 1));

  // Helper to perform spectrum interpolation given all relevant information;
  // - what tetrahedron of spectra to access
  // - what barycentrics to use
  vec4 detail_mix_reflectances(in vec4 wvls, in vec4 bary, in uint index) {
    vec4 r;
    for (uint i = 0; i < 4; ++i)
      r[i] = dot(bary, scene_spectral_data_texture(vec2(wvls[i], index)));
    return r;
  }

  vec4 scene_sample_reflectance(in uint object_i, in vec2 tx, in vec4 wvls) {
    // Load relevant info objects
    ObjectInfo      object_info       = scene_object_info(object_i);
    BarycentricInfo barycentrics_info = scene_reflectance_barycentric_info(object_i);

    // Translate gbuffer uv to texture atlas coordinate for the barycentrics;
    // also handle single-color objects by sampling the center of their patch
    vec2 tx_si = object_info.is_albedo_sampled ? tx : vec2(0.5f);
    vec3 tx_uv = vec3(barycentrics_info.uv0 + barycentrics_info.uv1 * tx_si, barycentrics_info.layer);

    uvec4 v = scene_barycentric_data_texture(tx_uv);
    return moments_to_reflectance(wvls, unpack_moments_12x10(v));

    // // Fill atlas texture coordinates, and spectral index
    // if (is_all_equal(ivec4(scene_barycentric_data_gather_w(tx_uv)))) { // Hot path; all element indices are the same, so use the one index for texture lookups
    //   // Sample barycentric weights
    //   vec4 bary   = scene_barycentric_data_texture(tx_uv);
    //   uint elem_i = uint(bary.w);
    //   bary.w      = 1.f - hsum(bary.xyz);

    //   // For each wvls, sample and compute reflectance
    //   // Reflectance is dot product of barycentrics and reflectances
    //   return detail_mix_reflectances(wvls, bary, elem_i);
    // } else {  
    //   // Scale up to full texture size
    //   vec3 tx    = tx_uv * vec3(scene_barycentric_data_size(), 1) - vec3(0.5, 0.5, 0);
    //   vec2 alpha = mod(tx.xy, 1.f);

    //   // Output reflectance, manual mixture of of four texels
    //   vec4 r = vec4(0); 
    //   for (uint i = 0; i < 4; ++i) {
    //     // Sample barycentric weights
    //     vec4 bary   = scene_barycentric_data_fetch(ivec3(tx) + ivec3(reflectance_tx_offsets[i], 0));
    //     uint elem_i = uint(bary.w);
    //     bary.w      = 1.f - hsum(bary.xyz);

    //     // For each of four wvls, sample and compute reflectance
    //     // Reflectance is dot product of barycentrics and reflectances
    //     r += hprod(mix(vec2(1) - alpha, alpha, vec2(reflectance_tx_offsets[i]))) 
    //        * detail_mix_reflectances(wvls, bary, elem_i);
    //   } // for (uint i)

    //   return r;
    // }
  }
#else  // SCENE_DATA_REFLECTANCE
  vec4 scene_sample_reflectance(in uint object_i, in vec2 tx, in vec4 wvls) { return vec4(1); }
#endif // SCENE_DATA_REFLECTANCE

#endif // REFLECTANCE_GLSL_GUARD