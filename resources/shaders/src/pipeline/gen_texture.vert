#include <preamble.glsl>
#include <math.glsl>
#include <render/detail/scene_types.glsl>

// Wrapper data packing tetrahedron data [x, y, z, w]; 64 bytes under std430
struct Elem {
  mat3 inv; // Inverse of 3x3 matrix [x - w, y - w, z - w]
  vec3 sub; // Subtractive component w
};

// General layout rule declarations
layout(std430) buffer;
layout(std140) uniform;

// Vertex stage declarations
layout(location = 0) in uvec4 in_vert_pack; // Packed (reparameterized) vertex data
layout(location = 1) in uint  in_txuv_pack; // Packed (original) texture UVs
layout(location = 0) out vec2 out_txuv;     // Per-vertex original texture UVs

// Uniform buffer declarations
layout(binding = 0) uniform b_buff_unif {
  uint  object_i;
  float px_scale;
} unif;
layout(binding = 1) uniform b_buff_atlas {
  uint n;
  AtlasLayout data[met_max_textures];
} buff_atlas;

void main() {
  // UV coordinates are directly unpacked and forwarded for texture sampling
  out_txuv = unpackUnorm2x16(in_txuv_pack);

  // Vertex position becomes reparatemerized texture coordinates in relevant atlas patch
  AtlasLayout atlas = buff_atlas.data[unif.object_i];
  gl_Position = vec4(atlas.uv0 + atlas.uv1 * unpackUnorm2x16(in_vert_pack.w), 0, 1) * 2.f - 1.f;
}