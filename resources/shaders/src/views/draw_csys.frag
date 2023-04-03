#version 460 core

// Enable early-z for this fragment shader
layout(early_fragment_tests) in;

// Input/output variables
layout(location = 0) in vec3  value_in;
layout(location = 0) out vec4 value_out;

// Uniform buffer declaration
layout(binding = 0, std140) uniform b_uniform {
  mat4  model_matrix;
  mat4  camera_matrix;
  float alpha;
} unif;

void main() {
  value_out = vec4(value_in, unif.alpha);
}
