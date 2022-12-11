#version 460 core

layout(location = 0) in  vec2 in_value_vert;
layout(location = 0) out vec4 out_value_colr;

void main() {
  if (length(in_value_vert) > 1.f) {
    discard;
  }

  out_value_colr = vec4(1);
}