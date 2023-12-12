#ifndef BVH_GLSL_GUARD
#define BVH_GLSL_GUARD

#include <gbuffer.glsl>
#include <ray.glsl>

uint BVHLeafFlagBit = 1u << 31;

struct BVHInfo {
  uint nodes_offs;
  uint nodes_size;
  uint prims_offs;
  uint prims_size;
};

struct BVHNode {
  BBox bbox;
  uint data0;
  uint data1;
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

struct BVHNodePack {
  uvec3 bbox;
  uint  data;
};

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

bool bvh_is_leaf(in BVHNode n) {
  return bool(n.data0 & BVHLeafFlagBit);
}

uint bvh_node_offs(in BVHNode n) {
  return n.data0;
}

uint bvh_node_size(in BVHNode n) {
  return n.data1;
}

uint bvh_prim_offs(in BVHNode n) {
  return (n.data0 & (~BVHLeafFlagBit));
}

uint bvh_prim_size(in BVHNode n) {
  return n.data1;
}

BVHNode unpack(in BVHNodePack p) {
  BVHNode o;
  
  vec2 mb     = unpackUnorm2x16(p.bbox[2]);
  o.bbox.minb = vec3(unpackUnorm2x16(p.bbox[0]), mb[0]);
  o.bbox.maxb = vec3(unpackUnorm2x16(p.bbox[1]), mb[1]);

  uint lf = p.data & BVHLeafFlagBit;
  o.data0 = ((p.data & (~BVHLeafFlagBit)) >> 4) | lf;
  o.data1 = p.data & 0xF;
  
  return o;
}

BVHVert unpack(in BVHVertPack p) {
  BVHVert o;
  o.p  = vec3(unpackUnorm2x16(p.p0), unpackUnorm2x16(p.p1).x);
  o.n  = decode_normal(unpackSnorm2x16(p.n));
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

#endif // BVH_GLSL_GUARD