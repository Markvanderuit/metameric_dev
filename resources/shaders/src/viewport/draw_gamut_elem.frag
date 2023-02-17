#version 460 core

// Enable early-z for this fragment shader
layout(early_fragment_tests) in;

// Input/output variables
layout(location = 0) in vec3  in_value_frag;
layout(location = 0) out vec4 out_value_colr;

// Uniform variables
layout(location = 0) uniform float u_value_offs  = 0.f;
layout(location = 1) uniform uint  u_use_opacity = 1;

// Buffer layout declarations
layout(std140) uniform;
layout(std430) buffer;

// Shader storage buffer declarations
layout(binding = 0) restrict readonly buffer b_0 { vec3  data[]; } b_posi_buffer;
layout(binding = 1) restrict readonly buffer b_1 { float data[]; } b_opac_buffer;
layout(binding = 2) restrict readonly buffer b_2 { float data[]; } b_size_buffer;

void main() {
  vec3 colr = vec3(u_value_offs) + (1.f - u_value_offs) * in_value_frag;
  float a = u_use_opacity == 0 ? 1.f : b_opac_buffer.data[gl_PrimitiveID];
  out_value_colr = vec4(colr, a);
}
