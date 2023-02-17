#version 460 core

#include <math.glsl>

layout(location = 0) uniform vec4 u_value;

// Fragment input declarations
layout(location = 0) in vec2 in_value_vert;

// Fragment output declarations
layout(location = 0) out vec4 out_value_colr;

void main() {
  if (sdot(in_value_vert) > 1)
    discard;

  out_value_colr = u_value;
}