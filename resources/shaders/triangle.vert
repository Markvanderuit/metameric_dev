#version 460 core

layout (location = 0) in vec3 position;
layout (location = 1) in vec3 color_in;
layout (location = 0) out vec3 color_out;
layout (location = 0) uniform float scalar;

void main() {
  color_out = color_in;
  gl_Position = vec4(scalar * position, 1.f);
}