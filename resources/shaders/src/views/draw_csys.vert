#include <preamble.glsl>

layout (location = 0) in vec3  value_in;
layout (location = 0) out vec3 value_out;

// Uniform buffer declaration
layout(binding = 0, std140) uniform b_uniform {
  mat4  model_matrix;
  mat4  camera_matrix;
  float alpha;
  vec3  color;
  bool  override_color;
} unif;

void main() {
  value_out = value_in;
  gl_Position = unif.camera_matrix * unif.model_matrix * vec4(value_in.xyz, 1);
}