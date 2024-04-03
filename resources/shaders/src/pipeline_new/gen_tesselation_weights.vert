#include <preamble.glsl>
#include <render/detail/scene_types.glsl>
#include <math.glsl>

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
layout(location = 0) in uvec4 in_vert_old; // Packed original mesh data
layout(location = 1) in uvec4 in_vert_new; // Reparameterized mesh data
layout(location = 0) out vec2 out_tx_old;  // Original texture UVs
layout(location = 1) out vec2 out_tx_new;  // Reparameterized texture UVs

void main() {

}