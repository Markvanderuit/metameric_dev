#ifndef LOAD_CMFS_BUCKETED_GLSL_GUARD
#define LOAD_CMFS_BUCKETED_GLSL_GUARD

#define SCENE_DATA_CMFS
#define SCENE_DATA_CMFS_BUCKETED

#define declare_scene_cmfs_data(scene_txtr_cmfs_data,                  \
                                bucket_i)                              \
  mat4x3 scene_cmfs(uint cmfs_i, vec4 wvls) {                          \
    return scene_txtr_cmfs_data[bucket_i];                             \
  }

#endif // LOAD_CMFS_BUCKETED_GLSL_GUARD