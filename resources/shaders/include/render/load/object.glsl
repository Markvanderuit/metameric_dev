#ifndef LOAD_OBJECT_GLSL_GUARD
#define LOAD_OBJECT_GLSL_GUARD

#define SCENE_DATA_OBJECT

#define declare_scene_object_data(info)                     \
  Object scene_object_info(uint i) { return info.data[i]; } \
  uint   scene_object_count()      { return info.n;       }
  
#endif // LOAD_OBJECT_GLSL_GUARD