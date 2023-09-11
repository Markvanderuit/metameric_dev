#ifndef SCENE_GLSL_GUARD
#define SCENE_GLSL_GUARD

// Info object for referred mesh data
struct MeshInfo {
  uint verts_offs; // Offset to vertex data range in vertex buffer
  uint verts_size; // Extent of vertex data range in vertex buffer
  uint elems_offs; // Offset to element data range in element buffer
  uint elems_size; // Extent of element data range in element buffer
};

// Info object to gather Scene::Object data
struct ObjectInfo {
  // Transform and inverse transform data
  mat4 trf;                
  mat4 trf_inv; 

  // Should the object be interacted with?
  uint is_active;       
  
  // Indices referring to auxiliary scene resources
  uint mesh_i;              
  uint uplifting_i;
  uint padd;

  // // Material data // TODO expand
  // uint  albedo_use_sampler; // Use sampler or direct value?
  // uint  albedo_i;           // Sampler index
  // vec3  albedo_v;           // Direct value
};

#endif // SCENE_GLSL_GUARD