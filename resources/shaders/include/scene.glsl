#ifndef SCENE_GLSL_GUARD
#define SCENE_GLSL_GUARD

// Define metameric's scene layout
const uint max_supported_meshes     = MET_SUPPORTED_MESHES;
const uint max_supported_objects    = MET_SUPPORTED_OBJECTS;
const uint max_supported_upliftings = MET_SUPPORTED_UPLIFTINGS;
const uint max_supported_textures   = MET_SUPPORTED_TEXTURES;

// Info object for referred mesh.bvh data
struct MeshInfo {
  mat4 trf;
  uint verts_offs;
  uint verts_size;

  uint prims_offs;
  uint prims_size;
  
  uint nodes_offs;
  uint nodes_size;
};


#define ObjectTypeSurface 0x01
#define ObjectTypeEmitter 0x02

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

#define EmitterTypeConstant 0x01
#define EmitterTypePoint    0x02
#define EmitterTypeArea     0x04

// Info object to gather Scene::Emitter data
// Given the lack of unions, emitters store additional data
struct EmitterInfo {
  // Type of emitter; constant, point, area
  uint type;

  // Should the emitter be interacted with?
  bool is_active;    

  // General data
  uint  illuminant_i;     // Index of spd data
  float illuminant_scale; // Scalar multiplier applied to spd samples

  // Point light data
  vec3 point_p;

  // Area light data
  mat4 area_trf;
  mat4 area_trf_inv; 
  uint area_mesh_i; // Index of attached surface              
};

// Atlas access info
struct AtlasLayout {
  uint  layer;  // layer in texture array in which the patch is located
  uvec2 offs;   // offset in pixels to texture's region storing this patch
  uvec2 size;   // size in pixels of texture's region storing this patch
  vec2  uv0;    // Minimum uv value, at region's offset
  vec2  uv1;    // Maximum uv value, at region's offset + size
};

#endif // SCENE_GLSL_GUARD