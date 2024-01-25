#ifndef RENDER_DETAIL_PACKING_GLSL_GUARD
#define RENDER_DETAIL_PACKING_GLSL_GUARD

#include <render/shape/aabb.glsl>

// Unpacked vertex data
struct MeshVert {
  vec3 p;
  vec3 n;
  vec2 tx;
};

// Unpacked primitive data, typically queried during bvh travesal
struct MeshPrim {
  MeshVert v0;
  MeshVert v1;
  MeshVert v2;
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

// BVH node data queries
bool bvh_is_leaf(in BVHNode node) { return bool(node.data_pack & (1u << 31)); } // Is leaf or inner node
uint bvh_offs(in BVHNode node)    { return (node.data_pack & (~(0xF << 27))); } // Node/prim offset
uint bvh_size(in BVHNode node)    { return (node.data_pack >> 27) & 0xF;      } // Node/prim count

// Extract a child AABB from a BVHNode
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

// Packed vertex data
struct MeshVertPack {
  uint p0; // unorm, 2x16
  uint p1; // unorm, 1x16 + padding 1x16
  uint n;  // snorm, 2x16, octagonal encoding
  uint tx; // unorm, 2x16
};

MeshVert unpack(in MeshVertPack p) {
  MeshVert o;
  o.p  = vec3(unpackUnorm2x16(p.p0),   unpackSnorm2x16(p.p1).x);
  o.n  = normalize(vec3(unpackSnorm2x16(p.p1).y, unpackSnorm2x16(p.n)));
  o.tx = unpackUnorm2x16(p.tx);
  return o;
}

// Packed primitive data, comprising three packed vertices,
// typically queried during bvh travesal
struct MeshPrimPack {
  MeshVertPack v0;
  MeshVertPack v1;
  MeshVertPack v2;
  uint padding[4]; // Brings alignment to 64 bytes
};

MeshPrim unpack(in MeshPrimPack p) {
  MeshPrim o;
  o.v0 = unpack(p.v0);
  o.v1 = unpack(p.v1);
  o.v2 = unpack(p.v2);
  return o;
}

// Packed BVH node data, comprising child AABBs and traversal data
struct BVHNodePack {
  uint aabb_pack_0;    // lo.x, lo.y
  uint aabb_pack_1;    // hi.x, hi.y
  uint aabb_pack_2;    // lo.z, hi.z
  uint data_pack;      // leaf | size | offs
  uint child_aabb0[8]; // per child: lo.x | lo.y | hi.x | hi.y
  uint child_aabb1[4]; // per child: lo.z | hi.z
};

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

#endif // RENDER_DETAIL_PACKING_GLSL_GUARD