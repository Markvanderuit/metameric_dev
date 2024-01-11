#ifndef BVH_GLSL_GUARD
#define BVH_GLSL_GUARD

#include <gbuffer.glsl>
#include <ray.glsl>

uint BVHLeafFlagBit = 1u << 31;

struct BVHVertPack {
  uint p0; // unorm, 2x16
  uint p1; // unorm, 1x16 + padding 1x16
  uint n;  // snorm, 2x16, octagohedral encoding
  uint tx; // unorm, 2x16
};

struct BVHPrimPack {
  BVHVertPack v0;
  BVHVertPack v1;
  BVHVertPack v2;
};

// Packed BVH node with compressed 8-child bounding boxes
// Packed size is 64 byes
struct BVHNodePack {
  uint aabb_pack_0;    // lo.x, lo.y
  uint aabb_pack_1;    // hi.x, hi.y
  uint aabb_pack_2;    // lo.z, hi.z
  uint data_pack;      // leaf | size | offs
  uint child_aabb0[8]; // per child: lo.x | lo.y | hi.x | hi.y
  uint child_aabb1[4]; // per child: lo.z | hi.z
};

struct BVHVert {
  vec3 p;
  vec3 n;
  vec2 tx;
};

struct BVHPrim {
  BVHVert v0;
  BVHVert v1;
  BVHVert v2;
};

// Partially unpacked BVH node with compressed 8-child bounding boxes
// Child AABBs are unpacked when necessary
struct BVHNode {
  // Unpacked helper data to unpack the rest
  vec3 aabb_minb;
  vec3 aabb_extn;

  // Remaining packed data
  uint data_pack;      // leaf | size | offs
  uint child_aabb0[8]; // per child: lo.x | lo.y | hi.x | hi.y
  uint child_aabb1[4]; // per child: lo.z | hi.z
};

// Node type; whether leaf or inner node
bool bvh_is_leaf(in BVHNode node) {
  return bool(node.data_pack & BVHLeafFlagBit);
}

// Node/primitive offset within BVH, independent of leaf or inner node
uint bvh_offs(in BVHNode node) {
  return (node.data_pack & (~(0xF << 27)));
}

// Node/primitive count within BVH, independent of leaf or inner node
uint bvh_size(in BVHNode node) {
  return (node.data_pack >> 27) & 0xF;
}

BVHVert unpack(in BVHVertPack p) {
  BVHVert o;
  o.p  = vec3(unpackUnorm2x16(p.p0), unpackUnorm2x16(p.p1).x);
  o.n  = unpack_snorm_3x32_octagonal(unpackSnorm2x16(p.n));
  o.tx = unpackUnorm2x16(p.tx);
  return o;
}

BVHPrim unpack(in BVHPrimPack p) {
  BVHPrim o;
  o.v0 = unpack(p.v0);
  o.v1 = unpack(p.v1);
  o.v2 = unpack(p.v2);
  return o;
}

BVHNode unpack(in BVHNodePack p) {
  BVHNode n;

  // Unpack auxiliary AABB data
  vec2 aabb_unpack_2 = unpackUnorm2x16(p.aabb_pack_2);
  n.aabb_minb = vec3(unpackUnorm2x16(p.aabb_pack_0), aabb_unpack_2.x);
  n.aabb_extn = vec3(unpackUnorm2x16(p.aabb_pack_1), aabb_unpack_2.y);
  
  // Transfer other data
  n.data_pack   = p.data_pack;
  n.child_aabb0 = p.child_aabb0;
  n.child_aabb1 = p.child_aabb1;
  
  return n;
}

AABB bvh_child_aabb(in BVHNode n, in uint i) {
  vec4 unpack_0 = unpackUnorm4x8(n.child_aabb0[i    ]);
  vec4 unpack_1 = unpackUnorm4x8(n.child_aabb1[i / 2] >> (bool(i % 2) ? 16 : 0));

  vec3 safe_minb = vec3(unpack_0.xy, unpack_1.x);
  vec3 safe_maxb = vec3(unpack_0.zw, unpack_1.y);

  AABB aabb;
  aabb.minb = n.aabb_minb + n.aabb_extn * safe_minb;
  aabb.maxb = n.aabb_minb + n.aabb_extn * safe_maxb;
  return aabb;
}

#endif // BVH_GLSL_GUARD