#version 460 core

#include <math.glsl>

layout(std430) buffer;

layout(location = 0) in vec2  in_value_vert; // Instanced, division 0
layout(location = 1) in vec3  in_value_posi; // Instanced, division 1
layout(location = 0) out vec2 out_value_vert;
layout(location = 1) out vec3 out_value_colr;

layout(binding = 0) restrict readonly buffer b_0 { vec3 data[]; } b_erro_buffer;

layout(location = 0) uniform mat4  u_camera_matrix;
layout(location = 1) uniform vec2  u_billboard_aspect;
layout(location = 2) uniform float u_point_radius_min;
layout(location = 3) uniform float u_point_radius_max;

void main() {
  float err = min(hsum(b_erro_buffer.data[gl_InstanceID]), 1.f);

  out_value_vert = in_value_vert;
  out_value_colr = mix(in_value_posi, vec3(1, 0, 0), err);

  float point_radius = mix(u_point_radius_min, u_point_radius_max, err);

  gl_Position = u_camera_matrix * vec4(in_value_posi, 1) 
              + point_radius * vec4(u_billboard_aspect * out_value_vert, 0, 0);
}