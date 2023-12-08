#version 460 core

#include <scene.glsl>
#include <gbuffer.glsl>

// Buffer layout declarations
layout(std140) uniform;
layout(std430) buffer;

// Vertex input declarations
layout(location = 0) in vec4 in_vert_a;
layout(location = 1) in vec4 in_vert_b;

// Vertex output declarations
layout(location = 0) out vec3      out_value_p;
layout(location = 1) out vec3      out_value_n;
layout(location = 2) out vec2      out_value_tx;
layout(location = 3) out flat uint out_value_id;

// Buffer declarations
layout(binding = 0) uniform b_buff_unif {
  mat4  trf;
} buff_unif;
layout(binding = 0) restrict readonly buffer b_buff_objects {
  ObjectInfo[] data;
} buff_objects;

void main() {
  // Unpack tightly packed vertex data to obtain vertex positions, normals and uvs
  out_value_p  = in_vert_a.xyz;
  out_value_n  = decode_normal(unpackSnorm2x16(floatBitsToUint(in_vert_b.x)));
  out_value_tx = unpackUnorm2x16(floatBitsToUint(in_vert_a.w));

  // The index in glMultiDraw* matches the index of a specific object
  out_value_id = gl_DrawID;
  
  // Vertex output is view * world * position
  gl_Position = buff_unif.trf 
              * buff_objects.data[gl_DrawID].trf
              * vec4(out_value_p, 1);
}