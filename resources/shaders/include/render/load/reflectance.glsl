#ifndef LOAD_REFLECTANCE_GLSL_GUARD
#define LOAD_REFLECTANCE_GLSL_GUARD

#define SCENE_DATA_REFLECTANCE

#define declare_scene_reflectance_data(scene_buff_bary_info,          \
                                       scene_txtr_bary_data,          \
                                       scene_txtr_spec_data,          \
                                       scene_txtr_coef_data,          \
                                       scene_txtr_warp_data)          \
  BarycentricInfo scene_reflectance_barycentric_info(uint object_i) { \
    return scene_buff_bary_info[object_i];                            \
  }                                                                   \
                                                                      \
  vec2 scene_barycentric_data_size() {                                \
    return vec2(textureSize(scene_txtr_bary_data, 0).xy);             \
  }                                                                   \
                                                                      \
  vec4 scene_barycentric_data_gather_w(vec3 p) {                      \
    return textureGather(scene_txtr_bary_data, p, 3);                 \
  }                                                                   \
                                                                      \
  vec4 scene_barycentric_data_texture(vec3 p) {                       \
    return texture(scene_txtr_bary_data, p);                          \
  }                                                                   \
                                                                      \
  vec4 scene_barycentric_data_fetch(ivec3 p) {                        \
    return texelFetch(scene_txtr_bary_data, p, 0);                    \
  }                                                                   \
                                                                      \
  vec4 scene_spectral_data_texture(vec2 p) {                          \
    return texture(scene_txtr_spec_data, p);                          \
  }                                                                   \
                                                                      \
  uvec4 scene_coefficients_data_texture(vec3 p) {                     \
    return texture(scene_txtr_coef_data, p);                          \
  }                                                                   \
                                                                      \
  float scene_phase_warp_data_texture(float p) {                      \
    return texture(scene_txtr_warp_data, p).x;                        \
  }
#endif // LOAD_REFLECTANCE_GLSL_GUARD