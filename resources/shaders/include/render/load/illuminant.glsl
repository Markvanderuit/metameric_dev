#ifndef LOAD_ILLUMINANT_GLSL_GUARD
#define LOAD_ILLUMINANT_GLSL_GUARD

#define SCENE_DATA_ILLUMINANT

#define declare_scene_illuminant_data(scene_txtr_illm_data)           \
  vec4 scene_illuminant(uint illm_i, vec4 wvls) {                     \
    vec4 v;                                                           \
    for (uint i = 0; i < 4; ++i)                                      \
      v[i] = texture(scene_txtr_illm_data, vec2(wvls[i], illm_i)).x;  \
    return v;                                                         \
  }
  
#endif // LOAD_ILLUMINANT_GLSL_GUARD