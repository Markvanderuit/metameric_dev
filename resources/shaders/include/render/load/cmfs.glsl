#ifndef LOAD_CMFS_GLSL_GUARD
#define LOAD_CMFS_GLSL_GUARD

#define SCENE_DATA_CMFS

#define declare_scene_cmfs_data(scene_txtr_cmfs_data)                  \
  mat4x3 scene_cmfs(uint cmfs_i, vec4 wvls) {                          \
    mat4x3 v;                                                          \
    for (uint i = 0; i < 4; ++i)                                       \
      v[i] = texture(scene_txtr_cmfs_data, vec2(wvls[i], cmfs_i)).xyz; \
    return v;                                                          \
  }

#endif // LOAD_CMFS_GLSL_GUARD