#ifndef RENDER_DETAIL_SCENE_TYPES_GLSL_GUARD
#define RENDER_DETAIL_SCENE_TYPES_GLSL_GUARD

// Define metameric's scene layout
const uint met_max_meshes      = MET_SUPPORTED_MESHES;
const uint met_max_objects     = MET_SUPPORTED_OBJECTS;
const uint met_max_emitters    = MET_SUPPORTED_EMITTERS;
const uint met_max_constraints = MET_SUPPORTED_CONSTRAINTS;
const uint met_max_textures    = MET_SUPPORTED_TEXTURES;

// Info object to gather Scene::Object data
// Material data is packed into a texture and unpacked to BRDF; 
// see bake_object_coef.comp/bake_object_brdf.comp
struct Object {
  mat4 trf;   // Transform and inverse transform data
  uint flags; // Object flags, see functions below
};

// Access packed object data
bool object_is_active(in Object ob) { return (ob.flags & 0x80000000) != 0; }
uint object_mesh_i(in Object ob)    { return (ob.flags & 0x7FFFFFFF);      }

// Info object to gather surface brdf data
struct BRDF {
  vec4  r;            // Albedo for 4 wavelengths
  uvec4 data;         // Compressed data pack containing most BRDF parameters
  float eta;          // Precomputed index of refraction for the hero wavelength
  bool  is_spectral;  // Is the BRDF wavelength-dependent?
};

// Access packed brdf data
float brdf_metallic(in BRDF brdf)     { return unpack_unorm_10((brdf.data.x) & 0x03FF); }
float brdf_transmission(in BRDF brdf) { return unpack_unorm_10((brdf.data.x >> 20) & 0x03FF); }
float brdf_absorption(in BRDF brdf)   { return unpackHalf2x16(((brdf.data.y >> 16) & 0xFFFFu)).x * 100.f; }
float brdf_clearcoat(in BRDF brdf)    { return unpack_unorm_10((brdf.data.z) & 0x03FF); }
float brdf_alpha(in BRDF brdf) { 
  float alpha = unpack_unorm_10((brdf.data.x >> 10) & 0x03FF); 
  return max(1e-3, alpha * alpha);
}
float brdf_clearcoat_alpha(in BRDF brdf)  { 
  float alpha = unpack_unorm_10((brdf.data.z >> 10) & 0x03FF);
  return max(1e-3, alpha * alpha);
}
vec3 brdf_normalmap(in BRDF brdf) {
  return unpack_normal_octahedral(unpackUnorm2x16(brdf.data.w));
}

// Info object to gather Scene::Emitter data
// Given the lack of unions, emitters store additional data
// Illuminant data is unpacked when necessary; see record.glsl
struct Emitter {
  #define EmitterTypeConstant           0
  #define EmitterTypePoint              1
  #define EmitterTypeSphere             2
  #define EmitterTypeRectangle          3
  // ---
  #define EmitterSpectrumTypeIlluminant 0
  #define EmitterSpectrumTypeColor      1

  mat4  trf;              // Transform data; sphere/rect position are extracted
  uint  flags;            // Emitter flags, see functions below
  float illuminant_scale; // Scalar multiplier  
  uint  illuminant_i;     // Index of spd
};

// Access packed emitter data
bool emitter_is_active(in Emitter em)  { return (em.flags & 0x80000000) != 0; }
uint emitter_spec_type(in Emitter em)  { return (em.flags & 0x0000FF00) >> 8; }
uint emitter_shape_type(in Emitter em) { return (em.flags & 0x000000FF); }

// Info object for referred patch from texture atlas
struct TextureInfo {
  bool  is_3f; // Is the patch in the atlas_3f texture sampler, or in atlas_1f?
  uint  layer; // layer in texture array in which the texture is located
  uvec2 offs;  // Offset to patch pixel region
  uvec2 size;  // Size of patch pixel region
  vec2  uv0;   // Minimum uv value, at region's pixel offset
  vec2  uv1;   // Maximum uv value, at region's pixel offset + size
};

// Info object for referred patch from texture atlas
struct AtlasInfo {
  uint  layer; // layer in texture array in which a texture patch is located
  uvec2 offs;  // Offset to patch pixel region
  uvec2 size;  // Size of patch pixel region
  vec2  uv0;   // Minimum uv value, at region's offset
  vec2  uv1;   // Maximum uv value, at region's offset + size
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