#ifndef LOAD_ILLUMINANT_BUCKETED_GLSL_GUARD
#define LOAD_ILLUMINANT_BUCKETED_GLSL_GUARD

#define SCENE_DATA_ILLUMINANT
#define SCENE_DATA_ILLUMINANT_BUCKETED

#define declare_scene_illuminant_data(scene_txtr_illm_data, \
                                      bucket_i)             \
  vec4 scene_illuminant(uint illm_i) {                      \
    return scene_txtr_illm_data[bucket_i][illm_i];          \
  }                                                         \
  vec4 scene_illuminant(uint illm_i, vec4 wvls) {           \
    return scene_txtr_illm_data[bucket_i][illm_i];          \
  }
  
#endif // LOAD_ILLUMINANT_BUCKETED_GLSL_GUARD