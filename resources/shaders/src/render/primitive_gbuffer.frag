#include <preamble.glsl>
#include <gbuffer.glsl>
#include <scene.glsl>

// Fragment early-Z declaration
layout(early_fragment_tests) in;

// Fragment input/output declarations
layout(location = 0) in  vec3      in_value_p;
layout(location = 1) in  flat uint in_value_i;
layout(location = 0) out uvec4     out_value;

// Uniform buffer declarations
layout(binding = 0) uniform b_buff_sensor {
  mat4  full_trf; 
  mat4  proj_trf;
  mat4  view_trf;
  uvec2 film_size; 
} buff_sensor;
layout(binding = 1) uniform b_buff_objects {
  uint n;
  ObjectInfo data[max_supported_objects];
} buff_objects;
layout(binding = 2) uniform b_buff_meshes {
  uint n;
  MeshInfo data[max_supported_meshes];
} buff_meshes;

void main() {
  // ObjectInfo object_info = buff_objects.data[in_value_i];
  // MeshInfo   mesh_info   = buff_meshes.data[object_info.mesh_i];
  GBufferRay gb = { 
    in_value_p, 
    in_value_i, /* << 24 | */
    /* mesh_info.prims_offs +  */gl_PrimitiveID /* ( & 0x00FFFFFF)  */// 8 bits for object id, 24 bits for primitive id
  };
  
  out_value = pack_gbuffer_ray(gb);
}