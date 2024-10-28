#ifndef LOAD_TLAS_GLSL_GUARD
#define LOAD_TLAS_GLSL_GUARD

#define SCENE_DATA_TLAS

#define declare_scene_tlas_data(scene_buff_tlas_prim,                     \
                                scene_buff_tlas_node,                     \
                                scene_buff)                               \
  uint        scene_tlas_prim(uint i) { return scene_buff_tlas_prim[i]; } \
  BVHNode     scene_tlas_node(uint i) { return scene_buff_tlas_node[i]; } \
  SceneInfo   scene_info()            { return scene_buff;              }
  
#define declare_scene_tlas_stack(stack_depth)                                                        \
  shared uint tlas_stack[gl_WorkGroupSize.x * gl_WorkGroupSize.y * gl_WorkGroupSize.z][stack_depth]; \
  void scene_set_tlas_stack_value(uint i, uint v) { tlas_stack[gl_LocalInvocationIndex][i] = v;    } \
  uint scene_get_tlas_stack_value(uint i)         { return tlas_stack[gl_LocalInvocationIndex][i]; }

#endif // LOAD_TLAS_GLSL_GUARD