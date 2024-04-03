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

// Specialization constant declarations
layout(constant_id = 0) const bool sample_albedo = true;

// Vertex stage declarations
layout(location = 0) in uvec4 in_vert_pack; // Packed (reparameterized) vertex data
layout(location = 1) in uint  in_txuv_pack; // Packed (original) texture UVs
layout(location = 0) out vec2 out_txuv;     // Per-vertex original texture UVs

// Storage buffer declarations
layout(binding = 0) restrict readonly buffer b_buff_atlas {
  AtlasLayout data[];
} buff_atlas;
layout(binding = 1) restrict readonly buffer b_buff_textures {
  TextureInfo[] data;
} buff_textures;

// Uniform buffer declarations
layout(binding = 0) uniform b_buff_unif {
  uint object_i;
  uint n;
} unif;
layout(binding = 1) uniform b_buff_uplift_data {
  uint offs;
  uint size;
} buff_uplift_data;
layout(binding = 2) uniform b_buff_uplift_pack { 
  Elem data[max_supported_constraints]; 
} buff_uplift_pack;
layout(binding = 3) uniform b_buff_objects {
  uint n;
  ObjectInfo data[max_supported_objects];
} buff_objects;

void main() {
  // Load relevant atlas data
  AtlasLayout atlas = buff_atlas.data[unif.object_i];

  // We compile-time select between single-color and texture
  // cases to avoid excessive warnings  when there is an 
  // unbound sampler object floating around
  if (sample_albedo) { // Texture case
    out_txuv = unpackUnorm2x16(in_txuv_pack); // Unpack position and forward
  } else { // Single-color case
    out_txuv = vec2(.5f); // Ignored
  }

  // Vertex position becomes reparatemerized texture coordinates
  gl_Position = vec4(atlas.uv0 + atlas.uv1 * unpackUnorm2x16(in_vert_pack.w), 0, 1) * 2.f - 1.f;
}