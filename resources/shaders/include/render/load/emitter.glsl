#ifndef LOAD_EMTITER_GLSL_GUARD
#define LOAD_EMTITER_GLSL_GUARD

#define SCENE_DATA_EMITTER

#define declare_scene_emitter_data(info)                      \
  Emitter scene_emitter_info(uint i) { return info.data[i]; } \
  uint   scene_emitter_count()       { return info.n;       }

#endif // LOAD_EMTITER_GLSL_GUARD