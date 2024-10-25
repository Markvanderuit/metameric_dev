#ifndef RENDER_DETAIL_SCENE_TYPES_GLSL_GUARD
#define RENDER_DETAIL_SCENE_TYPES_GLSL_GUARD

#include <spectrum.glsl>

// Define metameric's scene layout
const uint met_max_meshes      = MET_SUPPORTED_MESHES;
const uint met_max_objects     = MET_SUPPORTED_OBJECTS;
const uint met_max_emitters    = MET_SUPPORTED_EMITTERS;
const uint met_max_upliftings  = MET_SUPPORTED_UPLIFTINGS;
const uint met_max_constraints = MET_SUPPORTED_CONSTRAINTS;
const uint met_max_textures    = MET_SUPPORTED_TEXTURES;

// Info object to gather Scene::Object data
struct ObjectInfo {
  // Transform and inverse transform data
  mat4 trf;          // Object transform only        
  mat4 trf_mesh;     // Multiplied by mesh packing transform    
  mat4 trf_mesh_inv; // Multiplied by mesh packing transform

  // Should the object be interacted with?
  bool is_active;       
  
  // Indices referring to auxiliary scene resources
  uint mesh_i;              
  uint uplifting_i;

  // Material data
  uint  brdf_type;   // BRDF Type
  uvec4 albedo_data; // Albedo data record; see record.glsl to unpack a texture index or color value
};

// Info object for referred mesh.bvh data
struct MeshInfo {
  // Range of indices for vertex data
  uint verts_offs;
  uint verts_size;

  // Range of indices for primitive data
  uint prims_offs;
  uint prims_size;
  
  // Range of indices for blas node data
  uint nodes_offs;
  uint nodes_size;
};

// Info object for referred texture data
struct TextureInfo {
  bool  is_3f; // Is the patch in the atlas_3f texture sampler, or in atlas_1f?
  uint  layer; // layer in texture array in which the texture is located
  vec2  uv0;   // Minimum uv value, at region's pixel offset
  vec2  uv1;   // Maximum uv value, at region's pixel offset + size
};

// Info object for referred texture atlas patch data
struct TextureAtlasInfo {
  uint layer;  // layer in texture array in which a texture patch is located
  vec2 uv0;    // Minimum uv value, at region's offset
  vec2 uv1;    // Maximum uv value, at region's offset + size
};

#define EmitterTypeConstant  0
#define EmitterTypePoint     1
#define EmitterTypeSphere    2
#define EmitterTypeRectangle 3

// Info object to gather Scene::Emitter data
// Given the lack of unions, emitters store additional data
struct EmitterInfo {
  // Transform and inverse transform data
  mat4 trf;                
  mat4 trf_inv; 

  // Shape data
  uint type;      // Type of emitter; constant, point, area
  bool is_active; // Should the emitter be interacted with?

  // Spectral data
  uint  illuminant_i;     // Index of spd
  float illuminant_scale; // Scalar multiplier applied to values  
};

#define BRDFTypeNull       0
#define BRDFTypeDiffuse    1
#define BRDFTypeMirror     2
#define BRDFTypePrincipled 3

struct BRDFInfo {
  uint  type;      // Type of BRDF: null, diffuse, principled for now
  vec4  r;         // Underlying surface albedo for four wavelengths
  float alpha;     // Supplemental values for principled brdf
  float metallic;  // Supplemental values for principled brdf
  vec4  F0;        // Supplemental values for principled brdf
};

struct SceneInfo {
  // Transform and inverse transform data, mostly used for TLAS
  mat4 trf;
  mat4 trf_inv;
};

#include <render/detail/packing.glsl>

#endif // RENDER_DETAIL_SCENE_TYPES_GLSL_GUARD