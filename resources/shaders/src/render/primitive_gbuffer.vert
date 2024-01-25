#include <preamble.glsl>
#include <render/scene.glsl>

// Buffer layout declarations
layout(std140) uniform;
layout(std430) buffer;

// Vertex input/output declarations
layout(location = 0) in  uvec4     in_vert_pack; // bring back to v.p only?
layout(location = 0) out vec3      out_value_p;
layout(location = 1) out flat uint out_value_i;

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
  // Unpack packed vertex data to obtain local position
  vec4 value_p = vec4(unpackUnorm2x16(in_vert_pack.x).xy, unpackUnorm2x16(in_vert_pack.y).x, 1);
  
  // Set vertex outputs
  out_value_p = value_p.xyz;
  out_value_i = gl_DrawID; // The index in glMultiDraw* matches the index of a specific object

  // Apply camera transformation for vertex position output
  gl_Position = buff_sensor.full_trf 
              * buff_objects.data[gl_DrawID].trf_mesh
              * value_p;
}