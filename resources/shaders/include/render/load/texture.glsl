#ifndef LOAD_TEXTURE_GLSL_GUARD
#define LOAD_TEXTURE_GLSL_GUARD

#define SCENE_DATA_TEXTURE

#define declare_scene_texture_data(scene_buff_coef_info,    \
                                   scene_buff_brdf_info,    \
                                   scene_txtr_coef_data,    \
                                   scene_txtr_brdf_data,    \
                                   scene_txtr_bsis_data)    \
  AtlasInfo scene_texture_coef_info(uint object_i) {        \
    return scene_buff_coef_info[object_i];                  \
  }                                                         \
  AtlasInfo scene_texture_brdf_info(uint object_i) {        \
    return scene_buff_brdf_info[object_i];                  \
  }                                                         \
                                                            \
  vec2 scene_texture_coef_size() {                          \
    return vec2(textureSize(scene_txtr_coef_data, 0).xy);   \
  }                                                         \
  vec2 scene_texture_brdf_size() {                          \
    return vec2(textureSize(scene_txtr_brdf_data, 0).xy);   \
  }                                                         \
                                                            \
  uvec4 scene_texture_coef_fetch(ivec3 p) {                 \
    return texelFetch(scene_txtr_coef_data, p, 0);          \
  }                                                         \
  uint scene_texture_brdf_fetch(ivec3 p) {                  \
    return texelFetch(scene_txtr_brdf_data, p, 0).x;        \
  }                                                         \
                                                            \
  float scene_texture_basis_sample(float wvl, uint j) {     \
    return texture(scene_txtr_bsis_data, vec2(wvl, j)).x;   \
  }
  
#endif // LOAD_TEXTURE_GLSL_GUARD

