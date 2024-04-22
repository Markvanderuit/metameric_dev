#ifndef RENDER_DETAIL_PACKING_GLSL_GUARD
#define RENDER_DETAIL_PACKING_GLSL_GUARD

#include <spectrum.glsl>

// Packed vertex data
struct MeshVertPack {
  uint p0; // unorm, 2x16
  uint p1; // unorm, 1x16 + padding 1x16
  uint n;  // snorm, 2x16, octagonal encoding
  uint tx; // unorm, 2x16
};

// Packed primitive data, comprising three packed vertices,
// typically queried during bvh travesal
struct MeshPrimPack {
  MeshVertPack v0;
  MeshVertPack v1;
  MeshVertPack v2;
  uint padding[4]; // Brings alignment to 64 bytes
};

// Packed BVH node data, comprising child AABBs and traversal data
struct BVHNodePack {
  uint aabb_pack_0;    // lo.x, lo.y
  uint aabb_pack_1;    // hi.x, hi.y
  uint aabb_pack_2;    // lo.z, hi.z
  uint data_pack;      // leaf | size | offs
  uint child_aabb0[8]; // per child: lo.x | lo.y | hi.x | hi.y
  uint child_aabb1[4]; // per child: lo.z | hi.z
};

float[wavelength_bases] unpack_snorm_12(in uvec4 p) {
  float[wavelength_bases] m;
  for (int i = 0; i < 12; ++i) {
    uint j = bitfieldExtract(p[i / 3],              // 0,  0,  0,  1,  1,  1,  ...
                             i % 3 * 11,            // 0,  11, 22, 0,  11, 22, ...
                             i % 3 == 2 ? 10 : 11); // 11, 11, 10, 11, 11, 10, ...
    float scale = i % 3 == 2 ? 0.0009765625f : 0.0004882813f;
    m[i] = (float(j) * scale) * 2.f - 1.f;
  }
  return m;
}

uvec4 pack_snorm_12(in float[wavelength_bases] v) {
  uvec4 p;
  for (int i = 0; i < 12; ++i) {
    float scale = i % 3 == 2 ? 1024.f : 2048.f;
    uint j = uint(round((v[i] + 1.f) * .5f * scale));
    p[i / 3] = bitfieldInsert(p[i / 3],              // 0,  0,  0,  1,  1,  1,  ...
                              j,
                              i % 3 * 11,            // 0,  11, 22, 0,  11, 22, ...
                              i % 3 == 2 ? 10 : 11); // 11, 11, 10, 11, 11, 10, ...
  }
  return p;
}

float[8] unpack_half_8x16(in uvec4 p) {
  vec2 a = unpackHalf2x16(p[0]), b = unpackHalf2x16(p[1]),
       c = unpackHalf2x16(p[2]), d = unpackHalf2x16(p[3]);
  return float[8](a[0], a[1], b[0], b[1], c[0], c[1], d[0], d[1]);
}

uvec4 pack_half_8x16(in float[8] m) {
  return uvec4(packHalf2x16(vec2(m[0], m[1])), packHalf2x16(vec2(m[2], m[3])),
               packHalf2x16(vec2(m[4], m[5])), packHalf2x16(vec2(m[6], m[7])));
}

#endif // RENDER_DETAIL_PACKING_GLSL_GUARD