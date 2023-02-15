#version 460 core

#include <math.glsl>

// Triangle shape s.t. unculled fragments form an incircle with radius 1 and center (0, 0)
const vec2 elem_data[3] = vec2[3](
  vec2(-1.732, -1),
  vec2( 1.732, -1),
  vec2( 0,      2)
);

// Shader storage buffer declarations
layout(std430) buffer;
layout(binding = 0) restrict readonly buffer b_0 { uint data[]; } b_pack_buffer; // (r8 g8 b8 ...)
layout(binding = 1) restrict readonly buffer b_1 { vec4 data[]; } b_erro_buffer; // (r, g, b, sum)

// Uniform declarations
layout(location = 0) uniform mat4 u_camera_matrix;
/* layout(location = 1) uniform vec3 u_camera_position;
layout(location = 2) uniform vec3 u_camera_direction; */
layout(location = 3) uniform vec2 u_billboard_aspect;

// Vertex output declarations
layout(location = 0) out vec2 out_value_vert;
layout(location = 1) out vec3 out_value_colr;

/* // gl_PerVertex property declarations
out float gl_CullDistance[1]; */

// Internal constants
const float point_radius  = 0.002f;
const float cull_distance = 3.f;

void main() {
  uint i = gl_VertexID / 3, j = gl_VertexID % 3;

  vec3 pos = unpackUnorm4x8(b_pack_buffer.data[i]).xyz;

  out_value_vert = elem_data[j];
  out_value_colr = mix(pos, vec3(1, 0, 0), min(b_erro_buffer.data[i].w, 1.f));

  // Set per vertex position property
  gl_Position = u_camera_matrix * vec4(pos, 1) 
              + point_radius  * vec4(u_billboard_aspect * out_value_vert, 0, 0);

  // Set per vertex culling property
  // gl_CullDistance[0] = dot(u_camera_position - pos, u_camera_direction) > cull_distance ? -1.f : 1.f; 
}