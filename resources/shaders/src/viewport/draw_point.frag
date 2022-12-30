#version 460 core

layout(location = 0) in  vec2 in_value_vert;
layout(location = 0) out vec4 out_value_colr;

layout(location = 5) uniform vec4 u_value;

void main() {
  if (length(in_value_vert) > 1.f) {
    discard;
  }
  out_value_colr = u_value;
}