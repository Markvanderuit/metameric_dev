#version 460 core

// Input/output variables
layout(location = 0) in vec3  in_value_vert;
layout(location = 0) out vec3 out_value_vert;

// Buffer layout declarations
layout(std140) uniform;

// Uniform buffer declarations
layout(binding = 0) uniform u_0 {
  mat4 camera_matrix;
  vec2 camera_aspect;
} b_unif;

void main() {
  out_value_vert = in_value_vert;
  gl_Position = b_unif.camera_matrix * vec4(in_value_vert.xyz, 1);
}