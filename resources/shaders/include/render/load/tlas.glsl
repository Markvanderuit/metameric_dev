#ifndef LOAD_TLAS_GLSL_GUARD
#define LOAD_TLAS_GLSL_GUARD

#define SCENE_DATA_TLAS

#define declare_scene_tlas_data(scene_buff_tlas_prim,                     \
                                scene_buff_tlas_node,                     \
                                scene_buff_tlas_info)                     \
  TLASInfo    scene_tlas_info()       { return scene_buff_tlas_info;    } \
  uint        scene_tlas_prim(uint i) { return scene_buff_tlas_prim[i]; } \
  BVHNodePack scene_tlas_node(uint i) { return scene_buff_tlas_node[i]; }
  
#endif // LOAD_TLAS_GLSL_GUARD