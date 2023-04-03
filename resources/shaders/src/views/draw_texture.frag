#version 460 core

#include <math.glsl>

// Fragment input declarations
layout(location = 0) in vec2 in_value_vert;
layout(location = 1) in vec3 in_value_colr;

// Fragment output declarations
layout(location = 0) out vec4 out_value_colr;

void main() {
  if (sdot(in_value_vert) > 1)
    discard;

  out_value_colr = vec4(in_value_colr, 1);
}