#include <preamble.glsl>
#include <math.glsl>

layout(location = 0) in  vec2 in_value_vert;
layout(location = 0) out vec4 out_value_colr;

// Uniform buffer declaration
layout(binding = 0, std140) uniform b_uniform {
  mat4  model_matrix;
  mat4  camera_matrix;
  vec4  point_color;
  vec3  point_position;
  vec2  point_aspect;
  float point_size;
} unif;

void main() {
  if (sdot(in_value_vert) > 1)
    discard;
    
  out_value_colr = unif.point_color;
}