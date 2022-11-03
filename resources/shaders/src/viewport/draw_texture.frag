#version 460 core

layout(location = 0) in  vec2 in_value_vert;
layout(location = 1) in  vec3 in_value_posi;
layout(location = 0) out vec3 out_value_colr;

void main() {
  if (length(in_value_vert) > 1.f) {
    discard;
  }

  out_value_colr = in_value_posi;
}