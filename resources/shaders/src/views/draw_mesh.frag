#include <preamble.glsl>
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
layout(location = 3) in flat uint in_value_id;

// Fragment output declarations
layout(location = 0) out vec4 out_value_colr;

// Buffer declarations
layout(binding = 0) uniform b_unif_camera {
  mat4 trf;
} unif_camera;
layout(binding = 0) restrict readonly buffer b_buff_objects {
  ObjectInfo[] data;
} buff_objects;
layout(binding = 1) restrict readonly buffer b_buff_textures {
  TextureInfo[] data;
} buff_textures;

// Sampler declarations
layout(binding = 1) uniform sampler2DArray b_txtr_1f;
layout(binding = 2) uniform sampler2DArray b_txtr_3f;

vec4 sample_texture(uint texture_i, in vec2 tx_in) {
  TextureInfo info = buff_textures.data[texture_i];
  
  vec2 tx = info.uv0 + info.uv1 * mod(tx_in, 1);

  return info.is_3f ? texture(b_txtr_3f, vec3(tx, info.layer))
                    : texture(b_txtr_1f, vec3(tx, info.layer));
}

void main() {
  ObjectInfo object_info = buff_objects.data[in_value_id];

  // Hacky lambert
  vec3 l_dir      = normalize(vec3(1, 1, 1));
  vec3 n_dir      = normalize(in_value_n);
  float cos_theta = max(dot(l_dir, n_dir), 0);

  // Load diffuse data if provided
  vec3 diffuse = object_info.is_albedo_sampled
               ? sample_texture(object_info.albedo_i, in_value_tx).xyz
               : object_info.albedo_v;

  vec3 v = /* vec3(mod(in_value_tx, 1), 0); */  diffuse * cos_theta;
  out_value_colr = vec4(v, 1);
}