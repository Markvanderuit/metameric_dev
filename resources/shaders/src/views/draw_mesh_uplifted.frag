#version 460 core

#include <math.glsl>
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
layout(binding = 0) uniform b_unif {
  mat4 trf;
  float wvl;
} unif;
layout(binding = 0) restrict readonly buffer b_buff_objects {
  ObjectInfo[] data;
} buff_objects;
layout(binding = 1) restrict readonly buffer b_buff_textures {
  TextureInfo[] data;
} buff_textures;
layout(binding = 2) restrict readonly buffer b_buff_uplifts {
  UpliftInfo data[];
} buff_uplifts;

// Sampler declarations
layout(binding = 1) uniform sampler2DArray b_txtr_1f;
layout(binding = 2) uniform sampler2DArray b_txtr_3f;
layout(binding = 3) uniform sampler2DArray b_uplf_4f;
layout(binding = 4) uniform sampler1DArray b_spec_4f;
layout(binding = 5) uniform sampler1DArray b_illm_1f; // Illuminant function data, 1 component
layout(binding = 6) uniform sampler1DArray b_cmfs_3f; // Observer function data, 3 components
layout(binding = 7) uniform sampler1DArray b_csys_3f; // Color system spectra, 3 components

float sample_illuminant(in uint illuminant_i, float wvl) {
  return texture(b_illm_1f, vec2(wvl, illuminant_i)).x;
}

vec3 sample_observer(in uint observer_i, float wvl) {
  return texture(b_cmfs_3f, vec2(wvl, observer_i)).xyz;
}

vec3 sample_colsys(in uint colsys_i, float wvl) {
  return texture(b_csys_3f, vec2(wvl, colsys_i)).xyz;
}

vec3 sample_uplift(in ObjectInfo info, in vec2 tx_in) {
  UpliftInfo uplift_info = buff_uplifts.data[info.uplifting_i];
  
  // Query nearest weight for now
  uvec2 tx = info.offs + uvec2(vec2(info.size) * mod(tx_in, 1.f));
  vec4 bary = texelFetch(b_uplf_4f, ivec3(tx, info.layer), 0);

  // Extract index in tesselation and fill in weight w
  uint elem_i = floatBitsToUint(bary.w) + uplift_info.elem_offs;
  bary.w = 1.f - hsum(bary.xyz);

  // Quick hacky integration
  // TODO: switch to deferred rendering
  vec3 colr = vec3(0);
  uint n_samples = 0;
  for (float f = 0.0f; f < 1.0f; f += 0.01f) {
    float r = dot(bary, texture(b_spec_4f, vec2(/* unif.wvl */f, elem_i)));
    vec3 csys = sample_colsys(0, /* unif.wvl */f);
    colr += csys * r;
    n_samples++;
  }
  return colr;

  // // Sample spectrum at wavelength
  // vec3 csys = sample_colsys(0, unif.wvl);
  // // float l = sample_illuminant(0, unif.wvl);

  // return csys * r;
}

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
  vec3 diffuse = sample_uplift(object_info, in_value_tx);

  vec3 v = diffuse * cos_theta;
  out_value_colr = vec4(v, 1);
}