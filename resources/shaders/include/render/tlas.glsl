// #define RENDER_TLAS_GLSL_GUARD // disabled for now

#ifndef RENDER_TLAS_GLSL_GUARD
#define RENDER_TLAS_GLSL_GUARD

#include <render/bvh.glsl>
#include <render/object.glsl>
#include <render/emitter.glsl>

struct TLASPrimitive {
  bool is_object;
  uint object_i;
};

TLASPrimitive unpack_tlas_primitive(in uint pack) {
  TLASPrimitive prim;
  prim.is_object = record_is_object(pack);
  prim.object_i  = pack & 0x00FFFFFF;
  return prim;
}

void ray_intersect_tlas(inout Ray ray_world, in Ray ray) {
  // Shorthand // TODO remove and benchmark
  vec3 d_inv = 1.f / ray.d;

  // Initiate stack for traversal from root node
  // Stack values use 8 bits to flag nodes of interest, 
  // and 24 bits to store the offset to these nodes
  uvec4 stck;
  int   stckc = 0;
  stck[stckc] = 0x01000000;

  // Continue traversal until stack is once again empty
  while (stckc >= 0) {
    // Read offset and bitmask from stack
    uint node_first = stck[stckc] & 0x00FFFFFF;
    int  node_bit   = findMSB(stck[stckc] >> bvh_stck_offset);

    // Remove bit from mask on stack
    stck[stckc] &= (~(1u << (bvh_stck_offset + node_bit)));

    // If this was the last flagged bit, decrease stack count
    if ((stck[stckc] & 0xFF000000) == 0)
      stckc--;

    // Obtain and unpack next node
    uint node_i  = node_first + node_bit;
    BVHNode node = unpack(scene_tlas_node(node_i));

    if (bvh_is_leaf(node) || bvh_size(node) == 0) {
      // Range of primitives to intersect
      uint prim_begin = bvh_offs(node),
           prim_end   = prim_begin + bvh_size(node);

      // Iterate the node's primitives
      for (uint i = prim_begin; i < prim_end; ++i) {
        // Obtain and unpack next prim
        TLASPrimitive prim = unpack_tlas_primitive(scene_tlas_prim(i));

        // Then trace against it, dependent on type. On hit, copy back closest-hit information
        if (prim.is_object) {
          if (ray_intersect_object(ray_world, prim.object_i))
            ray_transform_inplace(ray, ray_world, scene_info().trf_inv);
        } else {
          if (ray_intersect_emitter(ray_world, prim.object_i))
            ray_transform_inplace(ray, ray_world, scene_info().trf_inv);
        }
      }
    } else {
      // Bitmask, initialized to all false
      uint mask = 0;
      
      // Iterate the node's children
      for (uint i = 0; i < bvh_size(node); ++i) {
        // Obtain and unpack next child, then test against it
        if (ray_intersect_any(ray, d_inv, bvh_child_aabb(node, i))) {
          // Flag child's index in bitmask on a hit
          mask |= (1u << i);
        }
      }

      // If any children were flagged in the mask, push the child offset + mask on the stack
      if (mask != 0) {
        stck[++stckc] = (mask << bvh_stck_offset) | bvh_offs(node);
      }
    }
  } // while (stckc >= 0)
}

bool ray_intersect_tlas_any(in Ray ray_world, in Ray ray) {
  // Shorthand // TODO remove and benchmark
  vec3 d_inv = 1.f / ray.d;

  // Initiate stack for traversal from root node
  // Stack values use 8 bits to flag nodes of interest, 
  // and 24 bits to store the offset to these nodes
  uvec4 stck;
  int   stckc = 0;
  stck[stckc] = 0x01000000;

  // Continue traversal until stack is once again empty
  while (stckc >= 0) {
    // Read offset and bitmask from stack
    uint node_first = stck[stckc] & 0x00FFFFFF;
    int  node_bit   = findMSB(stck[stckc] >> bvh_stck_offset);

    // Remove bit from mask on stack
    stck[stckc] &= (~(1u << (bvh_stck_offset + node_bit)));

    // If this was the last flagged bit, decrease stack count
    if ((stck[stckc] & 0xFF000000) == 0)
      stckc--;

    // Obtain and unpack next node
    uint node_i  = node_first + node_bit;
    BVHNode node = unpack(scene_tlas_node(node_i));

    if (bvh_is_leaf(node) || bvh_size(node) == 0) {
      // Range of primitives to intersect
      uint prim_begin = bvh_offs(node), 
           prim_end   = prim_begin + bvh_size(node);

      // Iterate the node's primitives
      for (uint i = prim_begin; i < prim_end; ++i) {
        // Obtain and unpack next prim
        TLASPrimitive prim = unpack_tlas_primitive(scene_tlas_prim(i));

        // Then trace against it, dependent on type
        if (prim.is_object) {
          if (ray_intersect_object_any(ray_world, prim.object_i))
            return true;
        } else {
          if (ray_intersect_emitter_any(ray_world, prim.object_i))
            return true;
        }
      }
    } else {
      // Bitmask, initialized to all false
      uint mask = 0;
      
      // Iterate the node's children
      for (uint i = 0; i < bvh_size(node); ++i) {
        // Obtain and unpack next child, then test against it
        if (ray_intersect_any(ray, d_inv, bvh_child_aabb(node, i))) {
          // Flag child's index in bitmask on a hit
          mask |= (1u << i);
        }
      }

      // If any children were flagged in the mask, push the child offset + mask on the stack
      if (mask != 0) {
        stck[++stckc] = (mask << bvh_stck_offset) | bvh_offs(node);
      }
    }
  } // while (stckc >= 0)

  return false;
}

#endif // RENDER_TLAS_GLSL_GUARD