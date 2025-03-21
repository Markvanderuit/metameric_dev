#ifndef RENDER_DETAIL_SCENE_TYPES_GLSL_GUARD
#define RENDER_DETAIL_SCENE_TYPES_GLSL_GUARD

// Define metameric's scene layout
const uint met_max_meshes      = MET_SUPPORTED_MESHES;
const uint met_max_objects     = MET_SUPPORTED_OBJECTS;
const uint met_max_emitters    = MET_SUPPORTED_EMITTERS;
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

// Info object for referred patch from texture atlas
struct TextureInfo {
  bool is_3f; // Is the patch in the atlas_3f texture sampler, or in atlas_1f?
  uint layer; // layer in texture array in which the texture is located
  uvec2 offs; // Offset to patch pixel region
  uvec2 size; // Size of patch pixel region
  vec2 uv0;   // Minimum uv value, at region's pixel offset
  vec2 uv1;   // Maximum uv value, at region's pixel offset + size
};

// Info object for referred patch from texture atlas
struct AtlasInfo {
  uint  layer; // layer in texture array in which a texture patch is located
  uvec2 offs;  // Offset to patch pixel region
  uvec2 size;  // Size of patch pixel region
  vec2  uv0;   // Minimum uv value, at region's offset
  vec2  uv1;   // Maximum uv value, at region's offset + size
};

// Info object to gather Scene::Emitter data
// Given the lack of unions, emitters store additional data
// Illuminant data is unpacked when necessary; see record.glsl
struct EmitterInfo {
  #define EmitterTypeConstant  0
  #define EmitterTypePoint     1
  #define EmitterTypeSphere    2
  #define EmitterTypeRectangle 3

  // Transform data; sphere/rect position are extracted
  mat4 trf;                

  // Shape data
  bool is_active; // Should the emitter be interacted with?
  uint type;      // Type of emitter; constant, point, sphere, rect

  // Spectral data
  uint  illuminant_i;     // Index of spd
  float illuminant_scale; // Scalar multiplier applied to values  
};


// Info object to gather brdf data locally
struct BRDFInfo {
  #define BRDFTypeNull       0
  #define BRDFTypeDiffuse    1
  #define BRDFTypeMicrofacet 2

  uint  type;     // Type of BRDF: null, diffuse, principled for now
  vec4  r;        // Underlying surface albedo for four wavelengths
  float alpha;    // Supplemental values for principled brdf
  float metallic; // Supplemental values for principled brdf
  vec4  F0;       // Supplemental values for principled brdf
};

// Info object for referred BLAS data
struct BLASInfo {
  uint prims_offs; // Offset into indices for bvh prim data
  uint nodes_offs; // Offset into indices for bvh node data
};

// Info object for the TLAS
struct TLASInfo {
  mat4 trf; // Transform to project ray from TLAS to world
};

#endif // RENDER_DETAIL_SCENE_TYPES_GLSL_GUARD