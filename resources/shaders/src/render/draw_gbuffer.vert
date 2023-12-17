#include <preamble.glsl>
#include <scene.glsl>

// Buffer layout declarations
layout(std140) uniform;
layout(std430) buffer;

// Vertex input/output declarations
layout(location = 0) in  uvec4     in_vert_pack; // bring back to v.p only?
layout(location = 0) out vec3      out_value_p;
layout(location = 1) out flat uint out_value_id;

// Uniform buffer declarations
layout(binding = 0) uniform b_buff_unif {
  mat4 trf;
} buff_unif;

// Storage buffer declarations
layout(binding = 0) restrict readonly buffer b_buff_objc { ObjectInfo[] data; } buff_objc;
layout(binding = 1) restrict readonly buffer b_buff_mesh { MeshInfo[]   data; } buff_mesh;

void main() {
  // The index in glMultiDraw* matches the index of a specific object
  uint object_i = gl_DrawID;
  uint mesh_i   = buff_objc.data[object_i].mesh_i;

  // Unpack packed vertex data to obtain positions, then
  // apply world and model transforms and retain homogeneous coordinate
  vec4 value_p = buff_objc.data[object_i].trf
               * buff_mesh.data[mesh_i].trf
               * vec4(unpackUnorm2x16(in_vert_pack.x).xy,
                      unpackUnorm2x16(in_vert_pack.y).x, 0);
  
  // Set vertex outputs
  out_value_p = value_p.xyz;
  out_value_id = object_i;

  // Vertex output is view * world * model * vertex input
  gl_Position = buff_unif.trf * value_p;
}