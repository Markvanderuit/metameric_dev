#version 460 core

#include <scene.glsl>

// Buffer layout declarations
layout(std140) uniform;
layout(std430) buffer;

// Fragment early-Z declaration
layout(early_fragment_tests) in;

// Fragment input declarations
layout(location = 0) in vec3 in_value_p;
layout(location = 1) in vec3 in_value_n;
layout(location = 2) in vec2 in_value_tx;

// Fragment output declarations
layout(location = 0) out vec4 out_value_colr;

// Buffer declarations
layout(binding = 0) uniform b_unif_camera {
  mat4 trf;
} unif_camera;
layout(binding = 0) restrict readonly buffer b_buff_objects {
  ObjectInfo[] data;
} buff_objects;

void main() {
  // Hacky lambert
  vec3 l_dir      = normalize(vec3(-1, 1, -1));
  vec3 n_dir      = normalize(in_value_n);
  float cos_theta = max(dot(l_dir, n_dir), 0);

  // Load diffuse data
  vec3 diffuse = vec3(1);
  // if (unif_object.use_diffuse_texture == 1) {
  //   diffuse = texture(b_diffuse_texture, in_value_tx).xyz;
  // } else {
  //   diffuse = unif_object.diffuse_value;
  // }

  vec3 v = diffuse * cos_theta;
  out_value_colr = vec4(v, 1);
}