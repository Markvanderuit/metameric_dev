#ifndef LOAD_RGB_GLSL_GUARD
#define LOAD_RGB_GLSL_GUARD

#define SCENE_DATA_RGB
#define SCENE_DATA_ILLUMINANT
                      
#define declare_scene_rgb_data(scene_buff_atls_info,                      \
                               scene_txtr_atls_data,                      \
                               scene_txtr_illm_data)                      \
  TextureInfo scene_rgb_atlas_info(uint object_i) {                       \
    return scene_buff_atls_info[object_i];                                \
  }                                                                       \
                                                                          \
  vec2 scene_rgb_data_size() {                                            \
    return vec2(textureSize(scene_txtr_atls_data, 0).xy);                 \
  }                                                                       \
                                                                          \
  vec4 scene_rgb_data_texture(vec3 p) {                                   \
    return texture(scene_txtr_atls_data, p);                              \
  }                                                                       \
                                                                          \
  vec4 scene_illuminant(uint illm_i, vec4 wvls) {                         \
    return vec4(texture(scene_txtr_illm_data, vec2(0.5, illm_i)).xyz, 1); \
  }
#endif // LOAD_RGB_GLSL_GUARD

