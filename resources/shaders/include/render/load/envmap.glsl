#ifndef LOAD_ENVMAP_GLSL_GUARD
#define LOAD_ENVMAP_GLSL_GUARD

#define declare_scene_envmap_data(info)                           \
  bool scene_has_envmap()       { return info.envm_is_present;  } \
  uint scene_envmap_emitter_i() { return info.envm_i;           }

#endif // LOAD_ENVMAP_GLSL_GUARD
