#version 460 core

// Vertex stage declaration
layout (location = 0) in vec3  value_in;
layout (location = 0) out vec4 value_out;

// Uniform buffer declaration
layout(binding = 0, std140) uniform b_uniform {
  mat4 model_matrix;
  mat4 camera_matrix;
  vec4 color_value;
} unif;

void main() {
  value_out = unif.color_value;
  gl_Position = unif.camera_matrix * unif.model_matrix * vec4(value_in.xyz, 1);
}
