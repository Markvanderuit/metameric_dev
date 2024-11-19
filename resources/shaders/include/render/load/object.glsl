#ifndef LOAD_OBJECT_GLSL_GUARD
#define LOAD_OBJECT_GLSL_GUARD

#define SCENE_DATA_OBJECT

#define declare_scene_object_data(scene_buff_objc_info,                    \
                                  scene_buff_objc_count)                   \
  ObjectInfo scene_object_info(uint i) { return scene_buff_objc_info[i]; } \
  uint scene_object_count()            { return scene_buff_objc_count;   }
  
#endif // LOAD_OBJECT_GLSL_GUARD