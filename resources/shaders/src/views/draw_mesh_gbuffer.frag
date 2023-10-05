#version 460 core

#include <scene.glsl>

// Buffer layout declarations
layout(std140) uniform;
layout(std430) buffer;

// Fragment early-Z declaration
layout(early_fragment_tests) in;

// Fragment input declarations
layout(location = 0) in vec3      in_value_p;
layout(location = 1) in vec3      in_value_n;
layout(location = 2) in vec2      in_value_tx;
layout(location = 3) in flat uint in_value_id;

// Fragment output declarations
layout(location = 0) out vec4 out_value_norm_dp; // Packed normal and linearized depth
layout(location = 1) out vec4 out_value_txc_idx; // Packed texture coord and object index

// Buffer declarations
layout(binding = 0) uniform b_buff_unif {
  mat4 trf;
  float z_near;
  float z_far;
} buff_unif;
layout(binding = 0) restrict readonly buffer b_buff_objects {
  ObjectInfo[] data;
} buff_objects;

float linearize_depth(float d, float z_near, float z_far) {
  float z_n = 2.0 * d - 1.0;
  return 2.0 * z_near * z_far / (z_far + z_near - z_n * (z_far - z_near));
}

void main() {
  float d = linearize_depth(gl_FragCoord.z, buff_unif.z_near, buff_unif.z_far);

  out_value_norm_dp = vec4(in_value_n, d);
  out_value_txc_idx = vec4(in_value_tx, uintBitsToFloat(in_value_id), 1);
}