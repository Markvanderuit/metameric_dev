#version 460 core

layout(location = 0) in vec3  in_value_vert;
layout(location = 0) out vec3 out_value_vert;

layout(location = 0) uniform mat4 u_model_matrix;
layout(location = 1) uniform mat4 u_camera_matrix;

void main() {
  out_value_vert = in_value_vert;
  gl_Position = u_camera_matrix * u_model_matrix * vec4(in_value_vert.xyz, 1);
}