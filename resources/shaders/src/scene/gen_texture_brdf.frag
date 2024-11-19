#include <preamble.glsl>
#include <math.glsl>
#include <render/record.glsl>
#include <render/load/defaults.glsl>

// General layout rule declarations
layout(std430) buffer;
layout(std140) uniform;

// Fragment stage declarations
layout(location = 0) in  vec2 in_txuv;    // Per-fragment original texture UVs, adjusted to atlas
layout(location = 0) out uint out_brdf; // Per fragment MESE representations of texel spectra

// Uniform buffer declarations
layout(binding = 0) uniform b_buff_unif {
  uint  object_i;
  float px_scale;
} unif;
/* layout(binding = 1) uniform b_buff_atlas {
  uint n;
  AtlasInfo data[met_max_textures];
} buff_atlas; */
layout(binding = 2) uniform b_buff_object_info {
  uint n;
  ObjectInfo data[met_max_objects];
} buff_objects;
layout(binding = 3) uniform b_buff_textures {
  uint n;
  TextureInfo data[met_max_textures];
} buff_textures;

// Image/sampler declarations
layout(binding = 0) uniform sampler2DArray b_txtr_3f;
layout(binding = 1) uniform sampler2DArray b_txtr_1f;

void main() {
  // Load relevant object data
  ObjectInfo object_info = buff_objects.data[unif.object_i];

  // Sample brdf data from object
  vec2 p = vec2(0);
  if (record_is_sampled(object_info.roughness_data)) {
    TextureInfo txtr = buff_textures.data[record_get_sampler_index(object_info.roughness_data)];
    p.x = txtr.is_3f
        ? texture(b_txtr_3f, vec3(txtr.uv0 + txtr.uv1 * in_txuv, txtr.layer)).x
        : texture(b_txtr_1f, vec3(txtr.uv0 + txtr.uv1 * in_txuv, txtr.layer)).x;
  } else {
    p.x = record_get_direct_value(object_info.roughness_data);
  }
  if (record_is_sampled(object_info.metallic_data)) {
    TextureInfo txtr = buff_textures.data[record_get_sampler_index(object_info.metallic_data)];
    p.y = txtr.is_3f
        ? texture(b_txtr_3f, vec3(txtr.uv0 + txtr.uv1 * in_txuv, txtr.layer)).x
        : texture(b_txtr_1f, vec3(txtr.uv0 + txtr.uv1 * in_txuv, txtr.layer)).x;
  } else {
    p.y =  record_get_direct_value(object_info.metallic_data);
  }
  
  // Output packed result
  out_brdf = packHalf2x16(p);
}