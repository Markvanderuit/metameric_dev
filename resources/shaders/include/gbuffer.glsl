#ifndef GLSL_GBUFFER_GUARD
#define GLSL_GBUFFER_GUARD

#include <math.glsl>

// Basic G-Buffer representation
struct GBuffer {
  vec3 p;        // World-space position
  vec3 n;        // World-space surface normal
  vec2 tx;       // Surface texture coordinate
  uint object_i; // Index of shape, UINT_MAX if no shape is present
};

float signNotZero(float f){
  return(f >= 0.0) ? 1.0 : -1.0;
}
vec2 signNotZero(vec2 v) {
  return vec2(signNotZero(v.x), signNotZero(v.y));
}

// Octagonal encoding for normal vectors; 3x32f -> 2x32f
vec2 encode_normal(vec3 n) {
  float l1 = abs(n.x) + abs(n.y) + abs(n.z);
  vec2 v = n.xy * (1.f / l1);
  if (n.z < 0.0) {
    v = (1.0 - abs(v.yx)) * signNotZero(v.xy);
  }
  return v;
}

// Octagonal decoding for normal vectors; 3x32f -> 2x32f
vec3 decode_normal(in vec2 v) {
  vec3 n = vec3(v.x, v.y, 1.0 - abs(v.x) - abs(v.y));
  if (n.z < 0) {
    n.xy = (1.0 - abs(n.yx)) * signNotZero(n.xy);
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

uvec4 encode_gbuffer(in float d, in vec3 n, in vec2 tx, in uint object_i) {
  uvec4 pack = uvec4(0);

  // 4 bytes, normal packs
  pack.x = packSnorm2x16(encode_normal(n));

  // 4 bytes, uv packing
  pack.y = packSnorm2x16(mod(tx, 1));
  
  // Other values stored directly
  pack.z = floatBitsToUint(d);
  pack.w = object_i;

  return pack;
}

GBuffer decode_gbuffer(in uvec4 v, in vec2 xy, in mat4 d_inv) {
  GBuffer gb;

  // Early out; return unspecified gbuffer if no values are set
  if (all(equal(v, vec4(0))))
    return GBuffer(vec3(0), vec3(0), vec2(0), UINT_MAX);

  // Unpack encoded values and assign directly stored values
  gb.n  = decode_normal(unpackSnorm2x16(v.x));
  gb.tx = unpackSnorm2x16(v.y);
  gb.object_i = v.w;

  // Recover world-space position from depth
  vec4 invp = d_inv * vec4(vec3(xy, v.z) * 2.f - 1.f, 1);
  gb.p = invp.xyz / invp.w;

  return gb;
}

#endif // GLSL_GBUFFER_GUARD
