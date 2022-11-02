#version 460 core

layout(location = 0) in  vec3 value_in;
layout(location = 0) out vec3 value_out;

void main() {
  value_out = value_in;
}