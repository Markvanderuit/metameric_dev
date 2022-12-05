#version 460 core

// Enable early-z for this fragment shader
layout(early_fragment_tests) in;

// Input/output variables
layout(location = 0) in vec3  in_value_frag;
layout(location = 0) out vec4 out_value_colr;

// Uniform variables
layout(location = 3) uniform float u_offset      = 1.f;
layout(location = 4) uniform uint  u_use_opacity = 1;

// Shader storage buffer declarations
layout(std430, binding = 0) restrict readonly buffer b_0 { float data[]; } b_opac;

void main() {
  vec3 colr = vec3(u_offset) + (1.f - u_offset) * in_value_frag;
  float a = u_use_opacity == 0 ? 1.f : b_opac.data[gl_PrimitiveID];
  out_value_colr = vec4(colr, a);
}
