#version 460 core

layout (location = 0) in vec3 gamut_value_in;
layout (location = 0) out vec3 gamut_value_out;

layout (location = 0) uniform mat4 camera_matrix;

void main() {
  gamut_value_out = gamut_value_in;
  gl_Position = camera_matrix * vec4(gamut_value_in, 1);
}