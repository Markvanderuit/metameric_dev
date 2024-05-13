#ifndef LOAD_REFLECTANCE_GLSL_GUARD
#define LOAD_REFLECTANCE_GLSL_GUARD

#define SCENE_DATA_REFLECTANCE

#define declare_scene_reflectance_data(scene_buff_atls_info,    \
                                       scene_txtr_bsis_data,    \
                                       scene_txtr_coef_data)    \
  BarycentricInfo scene_reflectance_atlas_info(uint object_i) { \
    return scene_buff_atls_info[object_i];                      \
  }                                                             \
                                                                \
  vec2 scene_barycentric_data_size() {                          \
    return vec2(textureSize(scene_txtr_coef_data, 0).xy);       \
  }                                                             \
                                                                \
  uvec4 scene_coefficients_data_texture(vec3 p) {               \
    return texture(scene_txtr_coef_data, p);                    \
  }                                                             \
                                                                \
  uvec4 scene_coefficients_data_fetch(ivec3 p) {                \
    return texelFetch(scene_txtr_coef_data, p, 0);              \
  }                                                             \
                                                                \
  float scene_basis_func(float wvl, uint j) {                   \
    return texture(scene_txtr_bsis_data, vec2(wvl, j)).x;       \
  }
#endif // LOAD_REFLECTANCE_GLSL_GUARD