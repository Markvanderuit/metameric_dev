#ifndef RENDER_DETAIL_SCENE_DECLARE_GLSL_GUARD
#define RENDER_DETAIL_SCENE_DECLARE_GLSL_GUARD

#define declare_scene_mesh_data(scene_buff_mesh_vert,                      \
                                scene_buff_mesh_elem,                      \
                                scene_buff_mesh_prim,                      \
                                scene_buff_mesh_node,                      \
                                scene_buff_mesh_info,                      \
                                scene_buff_mesh_count)                     \
  MeshVertPack scene_mesh_vert(uint i) { return scene_buff_mesh_vert[i]; } \
  uvec3 scene_mesh_elem(uint i)       { return scene_buff_mesh_elem[i]; }  \
  MeshPrimPack scene_mesh_prim(uint i) { return scene_buff_mesh_prim[i]; } \
  BVHNodePack scene_mesh_node(uint i) { return scene_buff_mesh_node[i]; }  \
  MeshInfo scene_mesh_info(uint i) { return scene_buff_mesh_info[i]; }     \
  uint scene_mesh_count() { return scene_buff_mesh_count; }

#define declare_scene_bvh_data(scene_buff_mesh_prim,                       \
                                scene_buff_mesh_node,                      \
                                scene_buff_mesh_info,                      \
                                scene_buff_mesh_count)                     \
  MeshPrimPack scene_mesh_prim(uint i) { return scene_buff_mesh_prim[i]; } \
  BVHNodePack scene_mesh_node(uint i) { return scene_buff_mesh_node[i]; }  \
  MeshInfo scene_mesh_info(uint i) { return scene_buff_mesh_info[i]; }     \
  uint scene_mesh_count() { return scene_buff_mesh_count; }

#define declare_scene_traversal_stack(stack_depth)                                                   \
  shared uint scene_stack[gl_WorkGroupSize.x * gl_WorkGroupSize.y *gl_WorkGroupSize.z][stack_depth]; \
  void scene_set_stack_value(uint i, uint v) { scene_stack[gl_LocalInvocationIndex][i] = v;    }     \
  uint scene_get_stack_value(uint i)         { return scene_stack[gl_LocalInvocationIndex][i]; }

#define declare_scene_emitter_data(scene_buff_emtr_info, scene_buff_emtr_count) \
  EmitterInfo scene_emitter_info(uint i) { return scene_buff_emtr_info[i]; }    \
  uint scene_emitter_count() { return scene_buff_emtr_count; }

#define declare_scene_object_data(scene_buff_objc_info, scene_buff_objc_count)  \
  ObjectInfo scene_object_info(uint i) { return scene_buff_objc_info[i]; }      \
  uint scene_object_count() { return scene_buff_objc_count; }

#define declare_scene_reflectance_data(scene_buff_bary_info,                                     \
                                       scene_txtr_bary_data,                                     \
                                       scene_txtr_spec_data)                                     \
  BarycentricInfo scene_reflectance_barycentric_info(uint i) { return scene_buff_bary_info[i]; } \
  sampler2DArray  scene_reflectance_barycentrics()           { return scene_txtr_bary_data;    } \
  sampler1DArray  scene_reflectance_spectra()                { return scene_txtr_spec_data;    }

#define declare_scene_cmfs_data(scene_txtr_cmfs_data)           \
  sampler1DArray cmfs_spectra() { return scene_txtr_cmfs_data; }

#define declare_scene_illuminant_data(scene_txtr_illm_data)           \
  sampler1DArray illuminant_spectra() { return scene_txtr_illm_data; }

#endif // RENDER_DETAIL_SCENE_DECLARE_GLSL_GUARD