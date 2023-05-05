#version 460 core

#include <math.glsl>

// Triangle shape s.t. unculled fragments form an incircle with radius 1 and center (0, 0)
const vec2 elem_data[3] = vec2[3](
  vec2(-1.732, -1),
  vec2( 1.732, -1),
  vec2( 0,      2)
);

// Layout declarations
layout(std140) uniform;
layout(std430) buffer;

// Buffer declarations
layout(binding = 0) restrict readonly buffer b_data_buffer { uint data[]; } data_buffer; // (r8 g8 b8 ...)
// layout(binding = 1) restrict readonly buffer b_erro_buffer { vec4 data[]; } erro_buffer; // (r, g, b, sum)
layout(binding = 0) uniform b_unif_buffer {
  mat4 camera_matrix;
  vec2 camera_aspect;
} unif;

// Vertex output declarations
layout(location = 0) out vec2 out_value_vert;
layout(location = 1) out vec3 out_value_colr;

// Internal constants
const float point_radius  = 0.001f;

void main() {
  uint i = gl_VertexID / 3, j = gl_VertexID % 3;

  vec3 pos = unpackUnorm4x8(data_buffer.data[i]).xyz;

  out_value_vert = elem_data[j];
  out_value_colr = pos; // mix(pos, vec3(1, 0, 0), min(erro_buffer.data[i].w, 1.f));

  // Set per vertex position property
  gl_Position = unif.camera_matrix * vec4(pos, 1) 
              + vec4(point_radius * unif.camera_aspect * out_value_vert, 0, 0);
}