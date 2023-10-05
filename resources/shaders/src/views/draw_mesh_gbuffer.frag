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
  mat4  trf;
  mat4  trf_inv; // TODO strip
  vec2  viewport_size;
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

// vec3 calculate_view_position(vec2 texture_coordinate, float depth_from_depth_buffer)
// {
//     vec3 clip_space_position = vec3(texture_coordinate, depth_from_depth_buffer) * 2.0 - vec3(1.0);
//     vec4 view_position = vec4(vec2(inverse_projection_matrix[0][0], inverse_projection_matrix[1][1]) * clip_space_position.xy,
//                                    inverse_projection_matrix[2][3] * clip_space_position.z + inverse_projection_matrix[3][3]);

//     return(view_position.xyz / view_position.w);
// }

// vec3 calculate_view_position(vec2 texture_coordinate, float depth, vec2 scale_factor)  // "scale_factor" is "v_fov_scale".
// {
//     vec2 half_ndc_position = vec2(0.5) - texture_coordinate;    // No need to multiply by two, because we already baked that into "v_tan_fov.xy".
//     vec3 view_space_position = vec3(half_ndc_position * scale_factor.xy * -depth, -depth); // "-depth" because in OpenGL the camera is staring down the -z axis (and we're storing the unsigned depth).
//     return view_space_position;
// }

void main() {
  float d = gl_FragCoord.z;
  // float d = linearize_depth(gl_FragCoord.z, buff_unif.z_near, buff_unif.z_far);
  
  // Recover position from depth
  // vec3 clipspace_pos = vec3(gl_FragCoord.xy / buff_unif.viewport_size, d) * 2.f - 1.f;
  // vec4 worldspace_pos = buff_unif.trf_inv * vec4(clipspace_pos, 1);
  // vec3 p = worldspace_pos.xyz / worldspace_pos.w;

  // Set debug output
  // out_value_norm_dp = vec4(p, 1);
  // out_value_txc_idx = vec4(in_value_p, 1);

  // Pack outputs s.t.
  // 0 : normal and linearized depth
  // 1 : texture coord and object index
  out_value_norm_dp = vec4(in_value_n, d);
  out_value_txc_idx = vec4(in_value_tx, uintBitsToFloat(in_value_id), 1);
}