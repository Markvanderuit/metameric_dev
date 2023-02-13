#version 460 core

layout(early_fragment_tests) in;

layout(location = 0) in  vec2 in_value_vert;
layout(location = 1) in  vec3 in_value_colr;
layout(location = 0) out vec4 out_value_colr;

void main() {
  if (dot(in_value_vert, in_value_vert) > 1)
    discard;

  out_value_colr = vec4(in_value_colr, 1);
}