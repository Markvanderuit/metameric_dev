#version 460 core

// Buffer layout declarations
layout(std140) uniform;
layout(std430) buffer;

// Fragment early-Z declaration
layout(early_fragment_tests) in;

// Fragment input declarations
layout(location = 0) in vec3 in_value_vert;
layout(location = 1) in vec3 in_value_norm;
layout(location = 2) in vec2 in_value_txuv;

// Fragment output declarations
layout(location = 0) out vec4 out_value_colr;

// Uniform declarations
layout(binding = 0) uniform b_unif {
  mat4 camera_matrix;
  mat4 model_matrix;
  uint use_diffuse_texture;
  vec3 diffuse_value;
} unif;
layout(binding = 1) uniform sampler2D b_diffuse_texture;

void main() {
  // Hacky lambert
  vec3 l_dir      = normalize(vec3(-1, 1, -1));
  vec3 n_dir      = normalize(in_value_norm);
  float cos_theta = max(dot(l_dir, n_dir), 0);

  // Load diffuse data
  vec3 diffuse;
  if (unif.use_diffuse_texture == 1) {
    diffuse = texture(b_diffuse_texture, in_value_txuv).xyz;
  } else {
    diffuse = unif.diffuse_value;
  }

  out_value_colr = vec4(diffuse * cos_theta, 1);
}