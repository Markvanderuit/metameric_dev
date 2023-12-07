#ifndef SCENE_GLSL_GUARD
#define SCENE_GLSL_GUARD

// Info object for referred mesh data
struct MeshInfo {
  uint verts_offs; // Offset to vert data range in verts buffer
  uint verts_size; // Extent of vert data range in verts buffer
  uint elems_offs; // Offset to elem data range in elems buffer
  uint elems_size; // Extent of elem data range in elems buffer
};

// Info object for referred texture data
struct TextureInfo {
  bool  is_3f; // Is in the atlas_3f texture sampler, else atlas_1f?
  uint  layer; // layer in texture array in which the texture is located
  uvec2 offs;  // offset in pixels to layer's region storing this texture
  uvec2 size;  // size in pixels of layer's region storing this
  vec2  uv0;   // Minimum uv value, at region's pixel offset
  vec2  uv1;   // Maximum uv value, at region's pixel offset + size
};

// Info object to gather Scene::Object data
struct ObjectInfo {
  // Transform and inverse transform data
  mat4 trf;                
  mat4 trf_inv; 

  // Should the object be interacted with?
  bool is_active;       
  
  // Indices referring to auxiliary scene resources
  uint mesh_i;              
  uint uplifting_i;

  // Material data        // TODO expand
  bool is_albedo_sampled; // Use sampler or direct value?
  uint albedo_i;          // Sampler index
  vec3 albedo_v;          // Direct value

  // Atlas access info
  uint layer;
  uvec2 offs;
  uvec2 size;
};

// Atlas access info
struct ObjectBaryInfo {
  uint layer;
  uvec2 offs;
  uvec2 size;
};

// Info object to gather Scene::Uplifting relevant data
struct UpliftInfo {
  uint elem_offs;
  uint elem_size;
};

#endif // SCENE_GLSL_GUARD