#version 460 core

// Enable early-z for this fragment shader
layout(early_fragment_tests) in;

// Input/output variables
layout(location = 0) in vec3  in_value_frag;
layout(location = 0) out vec4 out_value_colr;

// Uniform variables
layout(location = 0) uniform float u_value_offs = 0.f;

// Buffer layout declarations
layout(std140) uniform;
layout(std430) buffer;

void main() {
  vec3 colr = vec3(u_value_offs) + (1.f - u_value_offs) * in_value_frag;
  out_value_colr = vec4(colr, 1);
}
