#version 460 core

layout (location = 0) in vec3 gamut_value_in;
layout (location = 0) out vec3 color_value_out;

void main() {
  color_value_out = gamut_value_in;
}
