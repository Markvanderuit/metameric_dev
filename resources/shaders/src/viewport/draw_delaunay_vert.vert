#version 460 core

#include <math.glsl>

// Triangle shape s.t. unculled fragments form an incircle with radius 1 and center (0, 0)
const vec2 elem_data[3] = vec2[3](
  vec2(-1.732, -1),
  vec2( 1.732, -1),
  vec2( 0,      2)
);

// Buffer layout declarations
layout(std140) uniform;
layout(std430) buffer;

// Shader storage buffer declarations
layout(binding = 0) restrict readonly buffer b_0 { vec3  data[]; } b_posi_buffer;
layout(binding = 1) restrict readonly buffer b_1 { float data[]; } b_size_buffer;

// Uniform buffer declarations
layout(binding = 0) uniform u_0 {
  mat4 camera_matrix;
  vec2 camera_aspect;
} b_unif;

// Uniform declarations
layout(location = 0) uniform vec4 u_value;

// Vertex output declarations
layout(location = 0) out vec2 out_value_vert;

void main() {
  uint i = gl_VertexID / 3, j = gl_VertexID % 3;

  vec3 pos = b_posi_buffer.data[i];
  float size = b_size_buffer.data[i];

  out_value_vert = elem_data[j];

  gl_Position = b_unif.camera_matrix * vec4(pos, 1) 
              + size * vec4(b_unif.camera_aspect * out_value_vert, 0, 0);
}