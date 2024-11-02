#ifndef RENDER_BVH_GLSL_GUARD
#define RENDER_BVH_GLSL_GUARD

#include <render/ray.glsl>
#include <render/shape/aabb.glsl>
#include <render/shape/primitive.glsl>
#include <render/detail/mesh_packing.glsl>

// Hardcoded constants
#define bvh_stck_init_value 0x01000000
#define bvh_stck_offset     24

// BVH node data queries; operate on BVHNode0Pack
bool bvh_is_leaf(in BVHNode0Pack node) { return bool(node.data_pack & (1u << 31)); } // Is leaf or inner node
uint bvh_offs(in BVHNode0Pack node)    { return (node.data_pack & (~(0xF << 27))); } // Node/prim offset
uint bvh_size(in BVHNode0Pack node)    { return (node.data_pack >> 27) & 0xF;      } // Node/prim count

// Unpack a compressed AABB for the node parent
AABB unpack_parent_aabb(in BVHNode0Pack pack) {
  return AABB(vec3(unpackUnorm2x16(pack.aabb_pack[0]), unpackUnorm2x16(pack.aabb_pack[2]).x),
              vec3(unpackUnorm2x16(pack.aabb_pack[1]), unpackUnorm2x16(pack.aabb_pack[2]).y));
}

// Unpack a compressed AABB for one of the 8 children of a given parent
AABB unpack_child_aabb(in AABB parent_aabb, in BVHNode1Pack pack, in uint child_i) {
  uint pack_0   = pack.child_aabb0[child_i];
  uint pack_1   = pack.child_aabb1[child_i / 2] >> (bool(child_i % 2) ? 16 : 0);
  vec4 unpack_0 = unpackUnorm4x8(pack_0);
  vec4 unpack_1 = unpackUnorm4x8(pack_1);
  return AABB(fma(vec3(unpack_0.xy, unpack_1.x), parent_aabb.maxb, parent_aabb.minb),
              fma(vec3(unpack_0.zw, unpack_1.y), parent_aabb.maxb, parent_aabb.minb));
}

bool ray_intersect_bvh(inout Ray ray, in uint mesh_i) {
  // Return value
  bool hit = false;
  
  // Initiate small stack for traversal from root node
  // Stack values use 8 bits to flag nodes of interest, 
  // and 24 bits to store the offset to these nodes;
  // uvec4 stck = { bvh_stck_init_value, 0, 0, 0 };
  uint[6] stck;
  stck[0] = bvh_stck_init_value;

  // Obtain mesh information
  MeshInfo mesh_info = scene_mesh_info(mesh_i);
  
  // Continue traversal until stack is once again empty
  int stckc = 0; 
  while (stckc >= 0) {
    // Read next flagged bit and offset from stack, then remove flagged bit
    int  node_bit  = findMSB(stck[stckc] >> bvh_stck_offset);
    uint node_offs = stck[stckc] & 0x00FFFFFF;
    stck[stckc] &= (~(1u << (bvh_stck_offset + node_bit)));

    // If this was the last flagged bit, decrease stack count
    if ((stck[stckc] & 0xFF000000) == 0)
      stckc--;

    // Obtain next node parent data
    uint node_i       = mesh_info.nodes_offs + node_offs + node_bit;
    BVHNode0Pack node = scene_mesh_node0(node_i);

    if (bvh_size(node) == 0) {
      continue;
    } else if (bvh_is_leaf(node)) {
      // Iterate the node's primitives
      uint prim_begin = mesh_info.prims_offs + bvh_offs(node);
      for (uint prim_i = prim_begin; 
                prim_i < prim_begin + bvh_size(node);
              ++prim_i) {
        // Obtain and unpack next prim, then test against it
        Triangle prim = unpack_triangle(scene_mesh_prim(prim_i));
        if (ray_intersect(ray, prim)) {
          // Store primitive index on a hit
          record_set_object_primitive(ray.data, prim_i);
          hit = true;
        }
      }
    } else {
      // Load packed node child data
      AABB         parent_aabb = unpack_parent_aabb(node);
      BVHNode1Pack children    = scene_mesh_node1(node_i);

      // Bitmask, initialized to all false
      uint mask = 0;

      // Iterate the node's children
      for (uint child_i = 0;
                child_i < bvh_size(node);
              ++child_i) {
        // Unpack the next child, then test against it; 
        // on a hit, we flag its index in the bitmask
        AABB child_aabb = unpack_child_aabb(parent_aabb, children, child_i);
        if (ray_intersect_any(ray, child_aabb))
          mask |= 1u << child_i;
      } // for (uint i)

      // If any children were flagged in the mask, push the child offset + mask on the stack
      if (mask != 0)
        stck[++stckc] = (mask << bvh_stck_offset) | bvh_offs(node);
    }
  } // while (stckc >= 0)

  return hit;
}

bool ray_intersect_bvh_any(in Ray ray, in uint mesh_i) {
  // Initiate small stack for traversal from root node
  // Stack values use 8 bits to flag nodes of interest, 
  // and 24 bits to store the offset to these nodes;
  // uvec4 stck = { bvh_stck_init_value, 0, 0, 0 };
  uint[6] stck;
  stck[0] = bvh_stck_init_value;
  
  // Obtain mesh information
  MeshInfo mesh_info = scene_mesh_info(mesh_i);

  // Continue traversal until stack is once again empty
  int stckc = 0; 
  while (stckc >= 0) {
    // Read next flagged bit and offset from stack, then remove flagged bit
    int  node_bit  = findMSB(stck[stckc] >> bvh_stck_offset);
    uint node_offs = stck[stckc] & 0x00FFFFFF;
    stck[stckc] &= (~(1u << (bvh_stck_offset + node_bit)));

    // If this was the last flagged bit, decrease stack count
    if ((stck[stckc] & 0xFF000000) == 0)
      stckc--;

    // Obtain next node parent data
    uint node_i       = mesh_info.nodes_offs + node_offs + node_bit;
    BVHNode0Pack node = scene_mesh_node0(node_i);

    if (bvh_size(node) == 0) {
      continue;
    } else if (bvh_is_leaf(node)) {
      // Iterate the node's primitives
      uint prim_begin = mesh_info.prims_offs + bvh_offs(node);
      for (uint prim_i = prim_begin; 
                prim_i < prim_begin + bvh_size(node);
              ++prim_i) {
        // Obtain and unpack next prim, then test against it
        Triangle prim = unpack_triangle(scene_mesh_prim(prim_i));
        if (ray_intersect(ray, prim)) {
          return true;
        }
      }
    } else {
      // Load packed node child data
      AABB         parent_aabb = unpack_parent_aabb(node);
      BVHNode1Pack children    = scene_mesh_node1(node_i);

      // Bitmask, initialized to all false
      uint mask = 0;

      // Iterate the node's children
      for (uint child_i = 0;
                child_i < bvh_size(node);
              ++child_i) {
        // Unpack the next child, then test against it; 
        // on a hit, we flag its index in the bitmask
        AABB child_aabb = unpack_child_aabb(parent_aabb, children, child_i);
        if (ray_intersect_any(ray, child_aabb))
          mask |= 1u << child_i;
      } // for (uint i)

      // If any children were flagged in the mask, push the child offset + mask on the stack
      if (mask != 0)
        stck[++stckc] = (mask << bvh_stck_offset) | bvh_offs(node);
    }
  } // while (stckc >= 0)

  return false;
}

#endif // RENDER_BVH_GLSL_GUARD