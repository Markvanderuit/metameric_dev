#version 460 core

// Enable early-z for this fragment shader
layout(early_fragment_tests) in;

// Input/output variables
layout(location = 0) in vec3  value_in;
layout(location = 0) out vec4 value_out;

// Uniform variables
layout(location = 3) uniform float u_alpha = 1.f;

void main() {
  value_out = vec4(vec3(0.5f) + value_in, u_alpha);
}
