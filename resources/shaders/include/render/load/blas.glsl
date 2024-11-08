#ifndef LOAD_BLAS_GLSL_GUARD
#define LOAD_BLAS_GLSL_GUARD

#define SCENE_DATA_BLAS

#define declare_scene_blas_data(scene_buff_blas_prim,                         \
                                scene_buff_blas_node0,                        \
                                scene_buff_blas_node1,                        \
                                scene_buff_blas_info)                         \
  BLASInfo      scene_blas_info(uint i)  { return scene_buff_blas_info[i];  } \
  PrimitivePack scene_blas_prim(uint i)  { return scene_buff_blas_prim[i];  } \
  BVHNode0Pack  scene_blas_node0(uint i) { return scene_buff_blas_node0[i]; } \
  BVHNode1Pack  scene_blas_node1(uint i) { return scene_buff_blas_node1[i]; }
  
#endif // LOAD_BLAS_GLSL_GUARD