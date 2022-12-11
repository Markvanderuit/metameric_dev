#version 460 core

layout (location = 0) in vec3  value_in;
layout (location = 0) out vec4 value_out;

layout (location = 0) uniform mat4 u_model_matrix;
layout (location = 1) uniform mat4 u_camera_matrix;
layout (location = 2) uniform vec4 u_value;

void main() {
  value_out = u_value;
  gl_Position = u_camera_matrix * u_model_matrix * vec4(value_in.xyz, 1);
}
