#include <preamble.glsl>
#include <math.glsl>
#include <render/load/defaults.glsl>

// General layout rule declarations
layout(std430) buffer;
layout(std140) uniform;

// Vertex stage declarations
layout(location = 0) in uvec4 in_vert_pack; // Packed (reparameterized) vertex data
layout(location = 0) out vec2 out_txuv;     // Unpacked per-vertex texture UV data

// Uniform buffer declarations
layout(binding = 0) uniform b_buff_unif {
  uint  object_i;
  float px_scale;
} unif;
layout(binding = 1) uniform b_buff_atlas {
  uint n;
  AtlasInfo data[met_max_textures];
} buff_atlas;

void main() {
  // We pass on unpacked UV coordinates directly for texture sampling in the fragment stage
  out_txuv = unpackUnorm2x16(in_vert_pack.w);

  // The output position for the vertex stage is the UV coordinates mapped
  // to the relevant texture atlas patch of this object
  AtlasInfo atlas = buff_atlas.data[unif.object_i];
  gl_Position = vec4(atlas.uv0 + atlas.uv1 * out_txuv, 0, 1) * 2.f - 1.f;
}