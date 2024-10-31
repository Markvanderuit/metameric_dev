#include <preamble.glsl>
#include <math.glsl>
#include <render/record.glsl>
#include <render/detail/scene_types.glsl>

// General layout rule declarations
layout(std430) buffer;
layout(std140) uniform;

// Wrapper data representing tetrahedron data [x, y, z, w]
struct UpliftPack {
  mat3 inv; // Inverse of 3x3 matrix [x - w, y - w, z - w]
  vec3 sub; // Subtractive component w
};

// Fragment stage declarations
layout(location = 0) in  vec2  in_txuv;  // Per-fragment texture UVs, adjusted to atlas
layout(location = 0) out uvec4 out_coef; // Per fragment coefficients

// Uniform buffer declarations
layout(binding = 0) uniform b_buff_unif {
  uint  object_i;
  float px_scale;
} unif;
/* layout(binding = 1) uniform b_buff_atlas {
  uint n;
  AtlasInfo data[met_max_textures];
} buff_atlas; */
layout(binding = 2) uniform b_buff_uplift_data {
  uint offs;
  uint size;
} buff_uplift_data;
layout(binding = 3) uniform b_buff_uplift_pack { 
  UpliftPack data[met_max_constraints]; 
} buff_uplift_pack;
layout(binding = 4) uniform b_buff_objects {
  uint n;
  ObjectInfo data[met_max_objects];
} buff_objects;
layout(binding = 5) uniform b_buff_textures {
  uint n;
  TextureInfo data[met_max_textures];
} buff_textures;

// Storage buffer declarations
layout(binding = 0) restrict readonly buffer b_buff_uplift_coef { 
  float[met_max_constraints][4][wavelength_bases] data;
} buff_uplift_coef;

// Image/sampler declarations
layout(binding = 0) uniform sampler2DArray b_txtr_3f;
layout(binding = 1) uniform sampler2DArray b_txtr_1f;

void main() {
  // Load relevant object data
  ObjectInfo object_info = buff_objects.data[unif.object_i];

  // Sample color value from object
  vec3 p;
  if (record_is_sampled(object_info.albedo_data)) {
    // Color value is supplied by scene texture
    TextureInfo txtr = buff_textures.data[record_get_sampler_index(object_info.albedo_data)];
    p = txtr.is_3f
      ? texture(b_txtr_3f, vec3(txtr.uv0 + txtr.uv1 * in_txuv, txtr.layer)).xyz
      : vec3(texture(b_txtr_1f, vec3(txtr.uv0 + txtr.uv1 * in_txuv, txtr.layer)).x);
  } else {
    // Color value is specified directly
    p = record_get_direct_value(object_info.albedo_data);
  }
  
  // Next, brute-force search for the corresponding barycentric weights and tetrahedron's index
  float result_err = FLT_MAX;
  vec4  result_bary = vec4(0);
  uint  result_indx = 0;
  for (uint j = 0; j < buff_uplift_data.size; ++j) {
    // Compute barycentric weights using packed element data
    vec3 xyz  = buff_uplift_pack.data[j].inv * (p - buff_uplift_pack.data[j].sub);
    vec4 bary = vec4(xyz, 1.f - hsum(xyz));

    // Compute error of potentially unbounded barycentric weights
    float err = sdot(bary - clamp(bary, 0, 1));

    // Store better result if error is improved
    if (err > result_err)
      continue;
    result_err  = err;
    result_bary = bary;
    result_indx = j; // + buff_uplift_data.offs;
  } // for (uint j)

  // Then, gather basis coefficients representing tetrahedron's spectra and mix them
  float[wavelength_bases] coeffs;
  for (uint i = 0; i < wavelength_bases; ++i) {
    coeffs[i] = 0.f;
    for (uint j = 0; j < 4; ++j)
      coeffs[i] += result_bary[j] 
                 * buff_uplift_coef.data[result_indx][j][i];
  } // for (uint i)

  // Finally, store packed result
  out_coef = pack_basis_coeffs(coeffs);
}