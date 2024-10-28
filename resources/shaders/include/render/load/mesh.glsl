#ifndef LOAD_MESH_GLSL_GUARD
#define LOAD_MESH_GLSL_GUARD

#define SCENE_DATA_MESH

#define declare_scene_mesh_data(scene_buff_mesh_vert,                      \
                                scene_buff_mesh_elem,                      \
                                scene_buff_mesh_prim,                      \
                                scene_buff_mesh_node,                      \
                                scene_buff_mesh_info,                      \
                                scene_buff_mesh_count)                     \
  MeshVertPack scene_mesh_vert(uint i) { return scene_buff_mesh_vert[i]; } \
  uvec3 scene_mesh_elem(uint i)       { return scene_buff_mesh_elem[i]; }  \
  MeshPrimPack scene_mesh_prim(uint i) { return scene_buff_mesh_prim[i]; } \
  BVHNode  scene_mesh_node(uint i) { return scene_buff_mesh_node[i]; }  \
  MeshInfo scene_mesh_info(uint i) { return scene_buff_mesh_info[i]; }     \
  uint scene_mesh_count() { return scene_buff_mesh_count; }

#endif // LOAD_MESH_GLSL_GUARD