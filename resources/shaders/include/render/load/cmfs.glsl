#ifndef LOAD_CMFS_GLSL_GUARD
#define LOAD_CMFS_GLSL_GUARD

#define declare_scene_cmfs_data(scene_txtr_cmfs_data)                  \
  mat4x3 scene_cmfs(uint cmfs_i, vec4 wvls) {                          \
    mat4x3 v;                                                          \
    v[0] = texture(scene_txtr_cmfs_data, vec2(wvls[0], cmfs_i)).xyz;   \
    v[1] = texture(scene_txtr_cmfs_data, vec2(wvls[1], cmfs_i)).xyz;   \
    v[2] = texture(scene_txtr_cmfs_data, vec2(wvls[2], cmfs_i)).xyz;   \
    v[3] = texture(scene_txtr_cmfs_data, vec2(wvls[3], cmfs_i)).xyz;   \
    return v;                                                          \
  }

#endif // LOAD_CMFS_GLSL_GUARD