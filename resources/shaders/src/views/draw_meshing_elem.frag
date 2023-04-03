#version 460 core

// Enable early-z for this fragment shader
layout(early_fragment_tests) in;

// Fragment stage declarations
layout(location = 0) in vec3  value_in;
layout(location = 0) out vec4 value_out;

void main() {
  value_out = vec4(value_in, 1);
}
