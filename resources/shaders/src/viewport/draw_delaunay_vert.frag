#version 460 core

#include <math.glsl>

layout(location = 0) uniform vec4 u_value;

// Buffer layout declarations
layout(std140) uniform;

// Fragment stage declarations
layout(location = 0) in  vec2 value_in;
layout(location = 0) out vec4 value_out;

// Uniform buffer declarations
layout(binding = 0) uniform b_camera { // unused in stage
  mat4 matrix;
  vec2 aspect;
} camera;
layout(binding = 1) uniform b_value {
  vec4 value;
} unif;

void main() {
  if (sdot(value_in) > 1)
    discard;

  value_out = unif.value;
}