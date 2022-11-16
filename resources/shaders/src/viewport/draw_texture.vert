#version 460 core

layout(location = 0) in vec2  in_value_vert; // Instanced, division 0
layout(location = 1) in vec3  in_value_posi; // Instanced, division 1
layout(location = 0) out vec2 out_value_vert;
layout(location = 1) out vec3 out_value_posi;
layout(location = 2) flat out uint out_value_indx;

layout(location = 0) uniform mat4  u_camera_matrix;
layout(location = 1) uniform vec2  u_billboard_aspect;
layout(location = 2) uniform float u_point_radius;

void main() {
  out_value_vert = in_value_vert;
  out_value_posi = in_value_posi;
  out_value_indx = gl_InstanceID;

  gl_Position = u_camera_matrix * vec4(in_value_posi, 1) 
              + u_point_radius * vec4(u_billboard_aspect * out_value_vert, 0, 0);
}