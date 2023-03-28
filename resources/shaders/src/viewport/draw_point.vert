#version 460 core

layout(location = 0) in vec2  in_value_vert;
layout(location = 0) out vec2 out_value_vert;

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
  out_value_vert = in_value_vert;

  gl_Position = unif.camera_matrix * unif.model_matrix * vec4(unif.point_position, 1) 
              + unif.point_size * vec4(unif.point_aspect * out_value_vert, 0, 0);
}