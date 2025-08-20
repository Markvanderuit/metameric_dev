#ifndef LOAD_ENVMAP_GLSL_GUARD
#define LOAD_ENVMAP_GLSL_GUARD

#define SCENE_DATA_ENVMAP

#define declare_scene_envmap_data(info)                                  \
  bool scene_has_envmap()              { return info.envm_is_present;  } \
  uint scene_envmap_emitter_i()        { return info.envm_i;           } \
  uint scene_envmap_alias_table_size() { return info.alias_table_size; }

#endif // LOAD_ENVMAP_GLSL_GUARD
