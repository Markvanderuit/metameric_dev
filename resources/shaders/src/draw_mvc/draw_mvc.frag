#version 460 core

layout(location = 0) in  vec2 in_value_vert;
layout(location = 1) in  vec4 in_value_colr;
layout(location = 0) out vec4 out_value_colr;

void main() {
  if (length(in_value_vert) > 1.f || in_value_colr.w == 0.f) {
    discard;
  }

  out_value_colr = in_value_colr;
}