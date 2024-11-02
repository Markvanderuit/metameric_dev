#ifndef RENDER_DETAIL_SCENE_TYPES_GLSL_GUARD
#define RENDER_DETAIL_SCENE_TYPES_GLSL_GUARD

#include <spectrum.glsl>
#include <render/detail/mesh_packing.glsl>

// Define metameric's scene layout
const uint met_max_meshes      = MET_SUPPORTED_MESHES;
const uint met_max_objects     = MET_SUPPORTED_OBJECTS;
const uint met_max_emitters    = MET_SUPPORTED_EMITTERS;
const uint met_max_upliftings  = MET_SUPPORTED_UPLIFTINGS;
const uint met_max_constraints = MET_SUPPORTED_CONSTRAINTS;
const uint met_max_textures    = MET_SUPPORTED_TEXTURES;

// Info object to gather Scene::Object data
// Material data is unpacked when necessary; see record.glsl
struct ObjectInfo {
  // Transform and inverse transform data
  mat4 trf;

  // Should the object be interacted with?
  bool is_active;       
  
  // Indices referring to auxiliary scene resources
  uint mesh_i;              
  uint uplifting_i;

  // Material data
  uint  brdf_type;   // Type of brdf; 0 = null, 1 = diffuse, 2 = mirror, 3 = ggx
  uvec2 albedo_data;
  uint  metallic_data;
  uint  roughness_data;
};

// Info object for referred mesh.bvh data
struct MeshInfo {
  uint prims_offs; // Offset into indices for bvh prim data
  uint nodes_offs; // Offset into indices for bvh node data
};

// Info object for referred texture data
struct TextureInfo {
  bool is_3f; // Is the patch in the atlas_3f texture sampler, or in atlas_1f?
  uint layer; // layer in texture array in which the texture is located
  vec2 uv0;   // Minimum uv value, at region's pixel offset
  vec2 uv1;   // Maximum uv value, at region's pixel offset + size
};

// Info object for referred texture atlas patch data
struct AtlasInfo {
  uint  layer; // layer in texture array in which a texture patch is located
  uvec2 offs; // Offset to patch pixel region
  uvec2 size; // Size of patch pixel region
  vec2  uv0;   // Minimum uv value, at region's offset
  vec2  uv1;   // Maximum uv value, at region's offset + size
};

#define EmitterTypeConstant  0
#define EmitterTypePoint     1
#define EmitterTypeSphere    2
#define EmitterTypeRectangle 3

// Info object to gather Scene::Emitter data
// Given the lack of unions, emitters store additional data
// Illuminant data is unpacked when necessary; see record.glsl
struct EmitterInfo {
  // Transform data; sphere/rect position are extracted
  mat4 trf;                

  // Shape data
  uint type;      // Type of emitter; constant, point, sphere, rect
  bool is_active; // Should the emitter be interacted with?

  // Spectral data
  uint  illuminant_i;     // Index of spd
  float illuminant_scale; // Scalar multiplier applied to values  
};

#define BRDFTypeNull       0
#define BRDFTypeDiffuse    1
#define BRDFTypePrincipled 2

// Info object to gather brdf data locally
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
  mat4 inv;
};

#include <render/detail/packing.glsl>

#endif // RENDER_DETAIL_SCENE_TYPES_GLSL_GUARD