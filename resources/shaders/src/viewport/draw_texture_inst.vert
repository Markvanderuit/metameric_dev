#version 460 core

#include <math.glsl>

const vec2 elem_data[6] = vec2[6](
  vec2(-1, -1),
  vec2( 1, -1),
  vec2( 1,  1),
  vec2( 1,  1),
  vec2(-1,  1),
  vec2(-1, -1)
);

layout(std430) buffer;

layout(location = 0) out vec2 out_value_vert;
layout(location = 1) out vec3 out_value_colr;

layout(binding = 0) restrict readonly buffer b_0 { vec3 data[]; } b_posi_buffer;
layout(binding = 1) restrict readonly buffer b_1 { vec3 data[]; } b_erro_buffer;

layout(location = 0) uniform mat4  u_camera_matrix;
layout(location = 1) uniform vec2  u_billboard_aspect;
layout(location = 2) uniform float u_point_radius;

void main() {
  uint i = gl_VertexID / 6;
  uint j = gl_VertexID % 6;

  float err = min(hsum(b_erro_buffer.data[i]), 1.f);
  vec3  pos = b_posi_buffer.data[i];

  out_value_vert = elem_data[j];
  out_value_colr = mix(pos, vec3(1, 0, 0), err);

  gl_Position = u_camera_matrix * vec4(pos, 1) 
              + u_point_radius  * vec4(u_billboard_aspect * elem_data[j], 0, 0);
}