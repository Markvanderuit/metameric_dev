#ifndef LOAD_EMTITER_GLSL_GUARD
#define LOAD_EMTITER_GLSL_GUARD

#define SCENE_DATA_EMITTER

#define declare_scene_emitter_data(scene_buff_emtr_info,                                  \
                                   scene_buff_emtr_count,                                 \
                                   scene_buff_envm_info)                                  \
  EmitterInfo scene_emitter_info(uint i) { return scene_buff_emtr_info[i];              } \
  uint scene_emitter_count()             { return scene_buff_emtr_count;                } \
  bool scene_has_envm_emitter()          { return scene_buff_envm_info.envm_is_present; } \
  uint scene_envm_emitter_idx()          { return scene_buff_envm_info.envm_i;          }

#endif // LOAD_EMTITER_GLSL_GUARD