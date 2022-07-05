#version 460 core

layout (location = 0) in vec3  value_in;
layout (location = 0) out vec3 value_out;

layout (location = 0) uniform mat4 camera_matrix;

void main() {
  value_out = value_in;
  gl_Position = camera_matrix * vec4(value_in, 1);
}