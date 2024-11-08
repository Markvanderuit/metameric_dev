#ifndef LOAD_TLAS_GLSL_GUARD
#define LOAD_TLAS_GLSL_GUARD

#define SCENE_DATA_TLAS

#define declare_scene_tlas_data(scene_buff_tlas_prim,                        \
                                scene_buff_tlas_node0,                       \
                                scene_buff_tlas_node1,                       \
                                scene_buff_tlas_info)                        \
  TLASInfo     scene_tlas_info()        { return scene_buff_tlas_info;     } \
  uint         scene_tlas_prim(uint i)  { return scene_buff_tlas_prim[i];  } \
  BVHNode0Pack scene_tlas_node0(uint i) { return scene_buff_tlas_node0[i]; } \
  BVHNode1Pack scene_tlas_node1(uint i) { return scene_buff_tlas_node1[i]; }
  
#endif // LOAD_TLAS_GLSL_GUARD