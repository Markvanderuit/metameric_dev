#ifndef LOAD_EMTITER_GLSL_GUARD
#define LOAD_EMTITER_GLSL_GUARD

#define SCENE_DATA_EMITTER

#define declare_scene_emitter_data(scene_buff_emtr_info, scene_buff_emtr_count) \
  EmitterInfo scene_emitter_info(uint i) { return scene_buff_emtr_info[i]; }    \
  uint scene_emitter_count() { return scene_buff_emtr_count; } 
  
#endif // LOAD_EMTITER_GLSL_GUARD