#ifndef LOAD_REFLECTANCE_GLSL_GUARD
#define LOAD_REFLECTANCE_GLSL_GUARD

#define SCENE_DATA_REFLECTANCE

#define declare_scene_reflectance_data(scene_buff_bary_info,                                                      \
                                       scene_txtr_bary_data,                                                      \
                                       scene_txtr_spec_data)                                                      \
vec4 scene_sample_reflectance(in uint object_i, in vec2 tx, in vec4 wvls) {                                       \
  ObjectInfo      object_info       = scene_object_info(object_i);                                                \
  BarycentricInfo barycentrics_info = scene_buff_bary_info[object_i];                                             \
  vec2 tx_si = object_info.is_albedo_sampled ? tx : vec2(0.5f);                                                   \
  vec3 tx_uv = vec3(barycentrics_info.uv0 + barycentrics_info.uv1 * tx_si, barycentrics_info.layer);              \
  int  sp_index = -1;                                                                                             \
  vec3 tx_atlas = vec3(barycentrics_info.uv0 + barycentrics_info.uv1 * tx_si, barycentrics_info.layer);           \
  ivec4 indices = ivec4(textureGather(scene_txtr_bary_data, tx_atlas, 3));                                        \
  if (all(equal(indices, ivec4(indices[0])))) {                                                                   \
    sp_index = indices[0];                                                                                        \
  }                                                                                                               \
  vec4 r = vec4(0);                                                                                               \
  if (sp_index >= 0) {                                                                                            \
    vec4 bary = textureLod(scene_txtr_bary_data, tx_atlas, 0);                                                    \
    bary.w    = 1.f - hsum(bary.xyz);                                                                             \
    for (uint i = 0; i < 4; ++i) {                                                                                \
      vec4 refl = texture(scene_txtr_spec_data, vec2(wvls[i], sp_index));                                         \
      r[i] = dot(bary, refl);                                                                                     \
    }                                                                                                             \
  } else {                                                                                                        \
    const ivec2 tx_offsets[4] = ivec2[4](ivec2(0, 0), ivec2(1, 0), ivec2(0, 1), ivec2(1, 1));                     \
    vec3  tx       = tx_atlas * vec3(textureSize(scene_txtr_bary_data, 0).xy, 1) - vec3(0.5, 0.5, 0);             \
    ivec3 tx_floor = ivec3(tx);                                                                                   \
    vec2  alpha    = mod(tx.xy, 1.f);                                                                             \
                                                                                                                  \
    mat4 r_;                                                                                                       \
    for (uint i = 0; i < 4; ++i) {                                                                                \
      vec4 bary   = texelFetch(scene_txtr_bary_data, tx_floor + ivec3(tx_offsets[i], 0), 0);                      \
      uint elem_i = uint(bary.w);                                                                                 \
      bary.w      = 1.f - hsum(bary.xyz);                                                                         \
      for (uint j = 0; j < 4; ++j) {                                                                              \
        vec4 refl = texture(scene_txtr_spec_data, vec2(wvls[j], elem_i));                                         \
        r_[i][j] = dot(bary, refl);                                                                                \
      }                                                                                                           \
    }                                                                                                             \
                                                                                                                  \
    r = mix(mix(r_[0], r_[1], alpha.x), mix(r_[2], r_[3], alpha.x), alpha.y);                                         \
  }                                                                                                               \
                                                                                                                  \
  return r;                                                                                                       \
}

/* vec4 scene_sample_reflectance(in uint object_i, in vec2 tx, in vec4 wvls) {                                       
  // Load relevant info objects                                                                                   
  ObjectInfo      object_info       = scene_object_info(object_i);                                                
  BarycentricInfo barycentrics_info = scene_reflectance_barycentric_info(object_i);                               
                                                                                                                  
  // Translate gbuffer uv to texture atlas coordinate for the barycentrics;                                       
  // also handle single-color objects by sampling the center of their patch                                       
  vec2 tx_si = object_info.is_albedo_sampled ? tx : vec2(0.5f);                                                   
  vec3 tx_uv = vec3(barycentrics_info.uv0 + barycentrics_info.uv1 * tx_si, barycentrics_info.layer);              
                                                                                                                  
  // Fill atlas texture coordinates, and spectral index                                                           
  int  sp_index = -1;                                                                                             
  vec3 tx_atlas = vec3(barycentrics_info.uv0 + barycentrics_info.uv1 * tx_si, barycentrics_info.layer);           
  ivec4 indices = ivec4(textureGather(scene_reflectance_barycentrics(), tx_atlas, 3));                            
  if (all(equal(indices, ivec4(indices[0])))) {                                                                   
    sp_index = indices[0];                                                                                        
  }                                                                                                               
                                                                                                                  
  // Obtain spectral reflectance                                                                                  
  vec4 r = vec4(0);                                                                                               
  if (sp_index >= 0) {                                                                                            
    // Hot path; all element indices are the same, so use the one index for texture lookups                       
                                                                                                                  
    // Sample barycentric weights                                                                                 
    vec4 bary = textureLod(scene_reflectance_barycentrics(), tx_atlas, 0);                                        
    bary.w    = 1.f - hsum(bary.xyz);                                                                             
                                                                                                                  
    // For each wvls, sample and compute reflectance                                                              
    // Reflectance is dot product of barycentrics and reflectances                                                
    for (uint i = 0; i < 4; ++i) {                                                                                
      vec4 refl = texture(scene_reflectance_spectra(), vec2(wvls[i], sp_index));                                  
      r[i] = dot(bary, refl);                                                                                     
    } // for (uint i)                                                                                             
  } else {                                                                                                        
    // Cold path; element indices differ. Do costly interpolation manually :(                                     
                                                                                                                  
    const ivec2 tx_offsets[4] = ivec2[4](ivec2(0, 0), ivec2(1, 0), ivec2(0, 1), ivec2(1, 1));                     
                                                                                                                  
    // Scale up to full texture size                                                                              
    vec3  tx       = tx_atlas * vec3(textureSize(scene_reflectance_barycentrics(), 0).xy, 1) - vec3(0.5, 0.5, 0); 
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
                                                                                                                  
    r = mix(mix(r[0], r[1], alpha.x), mix(r[2], r[3], alpha.x), alpha.y);                                          
  }                                                                                                               
                                                                                                                  
  return r;                                                                                                       
} */
#endif // LOAD_REFLECTANCE_GLSL_GUARD