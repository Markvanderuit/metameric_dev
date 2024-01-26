#ifndef RENDER_DETAIL_PACKING_GLSL_GUARD
#define RENDER_DETAIL_PACKING_GLSL_GUARD

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

#endif // RENDER_DETAIL_PACKING_GLSL_GUARD