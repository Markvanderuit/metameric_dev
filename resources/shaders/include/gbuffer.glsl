#ifndef GLSL_GBUFFER_GUARD
#define GLSL_GBUFFER_GUARD

#include <math.glsl>

struct GBufferRay {
  vec3 p;           // World-space hit
  uint object_i;    // Index of hit object
  uint primitive_i; // Index of hit primitive in object
};

// Basic G-Buffer representation
struct GBuffer {
  vec3 p;        // World-space position
  vec3 n;        // World-space surface normal
  vec2 tx;       // Surface texture coordinate
  uint object_i; // Index of shape, UINT_MAX if no shape is present
};

float signNotZero(float f){
  return f >= 0.0 ? 1.0 : -1.0;
}

vec2 signNotZero(vec2 v) {
  return vec2(signNotZero(v.x), signNotZero(v.y));
}

// Octagonal encoding for normal vectors; 3x32f -> 2x32f
vec2 pack_snorm_2x32_octagonal(vec3 n) {
  vec2 v = n.xy  / hsum(abs(n));
  if (n.z < 0.f) {
    v = (1.f - abs(v.yx)) * signNotZero(v.xy);
  }
  return v;
}

// Octagonal encoding for normal vectors; 3x32f -> 2x32f
vec2 pack_unorm_2x32_octagonal(vec3 n) {
  vec2 v = n.xy  / hsum(abs(n));
  if (n.z < 0.f) {
    v = (1.f - abs(v.yx)) * signNotZero(v.xy);
  }
  return v * 0.5f + 0.5f;
}

// Octagonal decoding for normal vectors; 3x32f -> 2x32f
vec3 unpack_snorm_3x32_octagonal(in vec2 v) {
  vec3 n = vec3(v.xy, 1.f - hsum(abs(v.xy)));
  if (n.z < 0.f) {
    n.xy = (1.f - abs(n.yx)) * signNotZero(n.xy);
  }
  return normalize(n);
}

// Octagonal decoding for normal vectors; 3x32f -> 2x32f
vec3 unpack_unorm_3x32_octagonal(in vec2 v) {
  v = v * 2.f - 1.f;
  vec3 n = vec3(v.xy, 1.f - hsum(abs(v.xy)));
  if (n.z < 0.f) {
    n.xy = (1.f - abs(n.yx)) * signNotZero(n.xy);
  }
  return normalize(n);
}

// Encode 1x32f depth to two components using 16 bits
vec2 encode_depth(in float depth)
{
    float depthVal = depth * (256.0 * 256.0 - 1.0) / (256.0*256.0);
    vec3 encode = fract( depthVal * vec3(1.0, 256.0, 256.0*256.0) );
    return encode.xy - encode.yz / 256.0 + 1.0/512.0;
}

// Decode 1x32f depth from two components using 16 bits
float decode_depth(in vec2 pack)
{
    float depth = dot(pack, 1.0 / vec2(1.0, 256.0));
    return depth * (256.0*256.0) / (256.0*256.0 - 1.0);
}

// Generate packed data from gbuffer inputs
uvec4 encode_gbuffer(in float d, in vec3 n, in vec2 tx, in uint object_i) {
  uvec4 pack = uvec4(0);

  // 4 bytes, normal packs
  pack.x = packSnorm2x16(pack_unorm_2x32_octagonal(n));

  // 4 bytes, uv packing
  pack.y = packSnorm2x16(mod(tx, 1));
  
  // Other values stored directly
  pack.z = floatBitsToUint(d);
  pack.w = object_i;

  return pack;
}

// Generate gbuffer object from packed inputs
GBuffer decode_gbuffer(in uvec4 v, in vec2 xy, in mat4 d_inv) {
  GBuffer gb;

  // Early out; return unspecified gbuffer if no values are set
  if (all(equal(v, vec4(0))))
    return GBuffer(vec3(0), vec3(0), vec2(0), UINT_MAX);

  // Unpack encoded values and assign directly stored values
  gb.n  = unpack_unorm_3x32_octagonal(unpackUnorm2x16(v.x));
  gb.tx = unpackSnorm2x16(v.y);
  gb.object_i = v.w;

  // Recover world-space position from depth
  // There's def. cheaper ways to do this
  vec4 invp = d_inv * vec4(vec3(xy, uintBitsToFloat(v.z)) * 2.f - 1.f, 1);
  gb.p = invp.xyz / invp.w;

  return gb;
}

uvec4 pack_gbuffer_ray(in GBufferRay gb) {
  return uvec4(
    floatBitsToUint(gb.p),
    (gb.object_i & 0x000000FF) << 24 | (gb.primitive_i & 0x00FFFFFF)
  );
}

GBufferRay unpack_gbuffer_ray(in uvec4 v) {
  GBufferRay gb = { 
    uintBitsToFloat(v.xyz), 
    (v.w & 0xFF000000) >> 24,
    (v.w & 0x00FFFFFF)
  };
  return gb;
}

#endif // GLSL_GBUFFER_GUARD
