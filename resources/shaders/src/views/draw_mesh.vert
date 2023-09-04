#version 460 core

// Buffer layout declarations
layout(std140) uniform;
layout(std430) buffer;

// Vertex input declarations
layout(location = 0) in vec3 in_value_vert;
layout(location = 1) in vec3 in_value_norm;
layout(location = 2) in vec2 in_value_txuv;

// Vertex output declarations
layout(location = 0) out vec3 out_value_vert;
layout(location = 1) out vec3 out_value_norm;
layout(location = 2) out vec2 out_value_txuv;

// Uniform declarations
layout(binding = 0) uniform b_unif_camera {
  mat4 trf;
} unif_camera;
layout(binding = 1) uniform b_unif_object {
  mat4 trf;
  vec3 diffuse_value;
  uint use_diffuse_texture;
} unif_object;

void main() {
  out_value_vert = in_value_vert;
  out_value_norm = in_value_norm;
  out_value_txuv = in_value_txuv;
  
  gl_Position = unif_camera.trf * unif_object.trf * vec4(in_value_vert, 1);
}