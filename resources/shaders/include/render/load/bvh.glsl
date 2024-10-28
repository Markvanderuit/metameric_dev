#ifndef LOAD_BVH_GLSL_GUARD
#define LOAD_BVH_GLSL_GUARD

#define SCENE_DATA_MESH
#define SCENE_DATA_BVH

#define declare_scene_bvh_data(scene_buff_mesh_prim,                         \
                               scene_buff_mesh_node0,                        \
                               scene_buff_mesh_node1,                        \
                               scene_buff_mesh_info,                         \
                               scene_buff_mesh_count)                        \
  MeshPrimPack scene_mesh_prim(uint i) { return scene_buff_mesh_prim[i]; }   \
  BVHNode0Pack scene_mesh_node0(uint i) { return scene_buff_mesh_node0[i]; } \
  BVHNode1Pack scene_mesh_node1(uint i) { return scene_buff_mesh_node1[i]; } \
  MeshInfo     scene_mesh_info(uint i) { return scene_buff_mesh_info[i]; }   \
  uint scene_mesh_count() { return scene_buff_mesh_count; }
  
#endif // LOAD_BVH_GLSL_GUARD