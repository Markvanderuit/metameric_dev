#ifndef LOAD_BVH_GLSL_GUARD
#define LOAD_BVH_GLSL_GUARD

#define SCENE_DATA_MESH
#define SCENE_DATA_BVH

#define declare_scene_bvh_data(scene_buff_mesh_prim,                       \
                               scene_buff_mesh_node,                       \
                               scene_buff_mesh_info,                       \
                               scene_buff_mesh_count)                      \
  MeshPrimPack scene_mesh_prim(uint i) { return scene_buff_mesh_prim[i]; } \
  BVHNodePack scene_mesh_node(uint i) { return scene_buff_mesh_node[i]; }  \
  MeshInfo scene_mesh_info(uint i) { return scene_buff_mesh_info[i]; }     \
  uint scene_mesh_count() { return scene_buff_mesh_count; }
  
#define declare_scene_traversal_stack(stack_depth)                                                   \
  shared uint scene_stack[gl_WorkGroupSize.x * gl_WorkGroupSize.y *gl_WorkGroupSize.z][stack_depth]; \
  void scene_set_stack_value(uint i, uint v) { scene_stack[gl_LocalInvocationIndex][i] = v;    }     \
  uint scene_get_stack_value(uint i)         { return scene_stack[gl_LocalInvocationIndex][i]; }

#endif // LOAD_BVH_GLSL_GUARD