#ifndef RENDER_DETAIL_BVH_PACKING_GLSL_GUARD
#define RENDER_DETAIL_BVH_PACKING_GLSL_GUARD

// Packed vertex data
struct MeshVertPack {
  uint p0; // unorm, 2x16
  uint p1; // unorm, 1x16 + padding 1x16
  uint n;  // snorm, 2x16
  uint tx; // unorm, 2x16
};

// Packed primitive data, comprising three packed vertices,
// mostly queried during SurfaceInfo construction, and 
// partially queried during bvh traversal.
struct MeshPrimPack {
  MeshVertPack v0;
  MeshVertPack v1;
  MeshVertPack v2;
  uint padding[4]; // Aligned to 64 bytes
};

// Packed BVH node data, comprising child AABBs and traversal data
// First part, node AABB at half precision, and traversal data
struct BVHNode0Pack {
  uint aabb_pack[3]; // lo.x, lo.y | hi.x, hi.y | lo.z, hi.z
  uint data_pack;    // is_leaf | size | offs
};

// Second part, child AABBs, 8 bit precision
struct BVHNode1Pack {
  uint child_aabb0[8]; // 8 child aabbs: lo.x | lo.y | hi.x | hi.y
  uint child_aabb1[4]; // 8 child aabbs: lo.z | hi.z
};

#endif // RENDER_DETAIL_BVH_PACKING_GLSL_GUARD