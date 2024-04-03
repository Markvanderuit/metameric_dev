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

// Fragment stage declarations
layout(location = 0) in  vec2 in_txuv;    // Per-fragment original texture UVs, adjusted to atlas
layout(location = 0) out vec4 out_weight; // Per fragment barycentric coordinates and spectrum index

// Storage buffer declarations
layout(binding = 0) restrict readonly buffer b_buff_atlas {
  AtlasLayout data[];
} buff_weights;
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

// Image/sampler declarations
layout(binding = 0) uniform sampler2DArray b_txtr_3f;

void main() {
  // Load relevant object data
  ObjectInfo object = buff_objects.data[unif.object_i];

  // We compile-time select between single-color and texture
  // origins to avoid excessive warnings  when there is an 
  // unbound sampler object floating around
  vec3 p;
  if (sample_albedo) { 
    TextureInfo txtr = buff_textures.data[object.albedo_i];
    vec3 txuv = vec3(txtr.uv0 + txtr.uv1 * in_txuv, txtr.layer);

    // Color value is supplied by scene texture
    p = texture(b_txtr_3f, txuv).xyz;
  } else {
    // Color value is specified directly
    p = object.albedo_v;
  }
  
  // Next, brute-force search for the corresponding barycentric weights
  float result_err = FLT_MAX;
  vec4  result_bary;
  uint  result_indx;
  for (uint j = 0; j < buff_uplift_data.size; ++j) {
    // Compute barycentric weights using packed element data
    vec3 xyz  = buff_uplift_pack.data[j].inv * (p - buff_uplift_pack.data[j].sub);
    vec4 bary = vec4(xyz, 1.f - hsum(xyz));

    // Compute squared error of potentially unbounded barycentric weights
    float err = sdot(bary - clamp(bary, 0, 1));

    // Store better result if error is improved
    guard_continue(err < result_err);
    result_err  = err;
    result_bary = bary;
    result_indx = j + buff_uplift_data.offs;
  } // for (uint j)

  // Store result, packing 3/4th of the weights together with the tetrahedron's index
  out_weight = vec4(result_bary.xyz, float(result_indx));
}