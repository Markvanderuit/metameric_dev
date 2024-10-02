#ifndef RENDER_BVH_GLSL_GUARD
#define RENDER_BVH_GLSL_GUARD

#include <render/ray.glsl>
#include <render/shape/aabb.glsl>
#include <render/shape/primitive.glsl>

// Hardcoded constants
#define bvh_stck_offset 24

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

bool ray_intersect_bvh(inout Ray ray, in uint bvh_i) {
  vec3 d_inv = 1.f / ray.d;

  MeshInfo mesh_info = scene_mesh_info(bvh_i);

  // Initiate stack for traversal from root node
  // Stack values use 8 bits to flag nodes of interest, 
  // and 24 bits to store the offset to these nodes
  scene_set_stack_value(0u, 1u << bvh_stck_offset);
  uint stckc = 1; 
  bool hit   = false;

  // Continue traversal until stack is once again empty
  while (stckc > 0) {
    // Read offset and bitmask from stack
    uint stck_value = scene_get_stack_value(stckc - 1);
    uint node_first = stck_value & 0x00FFFFFF;
    int  node_bit   = findMSB(stck_value >> bvh_stck_offset);

    // Remove bit from mask on stack
    stck_value &= (~(1u << (bvh_stck_offset + node_bit)));
    scene_set_stack_value(stckc - 1, stck_value);

    // If this was the last flagged bit, decrease stack count
    if ((stck_value & 0xFF000000) == 0)
      stckc--;

    // Obtain and unpack next node
    BVHNode node = unpack(scene_mesh_node(mesh_info.nodes_offs + node_first + node_bit));

    if (bvh_is_leaf(node) || bvh_size(node) == 0) {
      // Iterate the node's primitives
      for (uint i = 0; i < bvh_size(node); ++i) {
        // Index of next primitive
        uint prim_i = mesh_info.prims_offs + bvh_offs(node) + i;
        
        // Obtain and unpack next prim
        PrimitivePositions prim = unpack_positions(scene_mesh_prim(prim_i));

        // Test against primitive; store primitive index on hit
        if (ray_intersect(ray, prim)) {
          record_set_object_primitive(ray.data, prim_i); // TODO ENABLE
          hit = true;
        }
      }
    } else {
      // Iterate the node's children, test against each AABB, and
      // on a hit flag the index of the child in a bitmask
      uint mask = 0;
      for (uint i = 0; i < bvh_size(node); ++i)
        if (ray_intersect_any(ray, d_inv, bvh_child_aabb(node, i)))
          mask |= (1u << i);

      // If any children were hit, indicated by bitflips, push the child offset + mask on the stack
      if (mask != 0) {
        stckc++;
        scene_set_stack_value(stckc - 1, (mask << bvh_stck_offset) | bvh_offs(node));
      }
    }
  } // while (stckc > 0)

  return hit;
}

bool ray_intersect_bvh_any(in Ray ray, in uint bvh_i) {
  vec3 d_inv = 1.f / ray.d;

  MeshInfo mesh_info = scene_mesh_info(bvh_i);

  // Initiate stack for traversal from root node
  // Stack values use 8 bits to flag nodes of interest, 
  // and 24 bits to store the offset to these nodes
  scene_set_stack_value(0u, 1u << bvh_stck_offset);
  uint stckc = 1;

  // Continue traversal until stack is once again empty
  while (stckc > 0) {
    // Read offset and bitmask from stack
    uint stck_value = scene_get_stack_value(stckc - 1);
    uint node_first = stck_value & 0x00FFFFFF;
    int  node_bit   = findMSB(stck_value >> bvh_stck_offset);

    // Remove bit from mask on stack
    stck_value &= (~(1u << (bvh_stck_offset + node_bit)));
    scene_set_stack_value(stckc - 1, stck_value);

    // If this was the last flagged bit, decrease stack count
    if ((stck_value & 0xFF000000) == 0)
      stckc--;

    // Obtain and unpack next node
    BVHNode node = unpack(scene_mesh_node(mesh_info.nodes_offs + node_first + node_bit));

    if (bvh_is_leaf(node) || bvh_size(node) == 0) {
      // Iterate the node's primitives
      for (uint i = 0; i < bvh_size(node); ++i) {
        // Obtain and unpack next prim
        PrimitivePositions prim = unpack_positions(scene_mesh_prim(mesh_info.prims_offs + bvh_offs(node) + i));

        // Test against primitive; store primitive index on hit
        if (ray_intersect(ray, prim)) {
          return true;
        }
      }
    } else {
      // Iterate the node's children, test against each AABB, and
      // on a hit flag the index of the child in a bitmask
      uint mask = 0;
      for (uint i = 0; i < bvh_size(node); ++i) {
        if (ray_intersect_any(ray, d_inv, bvh_child_aabb(node, i)))
          mask |= (1u << i);
      } // for (uint i)

      // If any children were hit, indicated by bitflips, push the child offset + mask on the stack
      if (mask != 0) {
        stckc++;
        scene_set_stack_value(stckc - 1, (mask << bvh_stck_offset) | bvh_offs(node));
      }
    }
  } // while (stckc > 0)

  return false;
}

#endif // RENDER_BVH_GLSL_GUARD