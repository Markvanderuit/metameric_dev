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
  vec3 d_inv = 1.f / ray.d;

  // Initiate stack for traversal from root node
  // Stack values use 8 bits to flag nodes of interest, 
  // and 24 bits to store the offset to these nodes
  scene_set_tlas_stack_value(0u, 1u << bvh_stck_offset);
  uint stckc = 1; 

  // Continue traversal until stack is once again empty
  while (stckc > 0) {
    // Read offset and bitmask from stack
    uint stck_value = scene_get_tlas_stack_value(stckc - 1);
    uint node_first = stck_value & 0x00FFFFFF;
    int  node_bit   = findMSB(stck_value >> bvh_stck_offset);

    // Remove bit from mask on stack
    stck_value &= (~(1u << (bvh_stck_offset + node_bit)));
    scene_set_tlas_stack_value(stckc - 1, stck_value);

    // If this was the last flagged bit, decrease stack count
    if ((stck_value & 0xFF000000) == 0)
      stckc--;

    // Obtain and unpack next node
    BVHNode node = unpack(scene_tlas_node(node_first + node_bit));

    if (bvh_is_leaf(node) || bvh_size(node) == 0) {
      // Iterate the node's primitives
      for (uint i = 0; i < bvh_size(node); ++i) {
        // Obtain and unpack next prim
        TLASPrimitive prim = unpack_tlas_primitive(scene_tlas_prim(bvh_offs(node) + i));

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
      // Iterate the node's children, test against each AABB, and
      // on a hit flag the index of the child in a bitmask
      uint mask = 0;
      for (uint i = 0; i < bvh_size(node); ++i)
        if (ray_intersect_any(ray, d_inv, bvh_child_aabb(node, i)))
          mask |= (1u << i);

      // If any children were hit, indicated by bitflips, push the child offset + mask on the stack
      if (mask != 0) {
        stckc++;
        scene_set_tlas_stack_value(stckc - 1, (mask << bvh_stck_offset) | bvh_offs(node));
      }
    }
  } // while (stckc > 0)
}

bool ray_intersect_tlas_any(in Ray ray_world, in Ray ray) {
  vec3 d_inv = 1.f / ray.d;

  // Initiate stack for traversal from root node
  // Stack values use 8 bits to flag nodes of interest, 
  // and 24 bits to store the offset to these nodes
  scene_set_tlas_stack_value(0u, 1u << bvh_stck_offset);
  uint stckc = 1; 

  // Continue traversal until stack is once again empty
  while (stckc > 0) {
    // Read offset and bitmask from stack
    uint stck_value = scene_get_tlas_stack_value(stckc - 1);
    uint node_first = stck_value & 0x00FFFFFF;
    int  node_bit   = findMSB(stck_value >> bvh_stck_offset);

    // Remove bit from mask on stack
    stck_value &= (~(1u << (bvh_stck_offset + node_bit)));
    scene_set_tlas_stack_value(stckc - 1, stck_value);

    // If this was the last flagged bit, decrease stack count
    if ((stck_value & 0xFF000000) == 0)
      stckc--;

    // Obtain and unpack next node
    BVHNode node = unpack(scene_tlas_node(node_first + node_bit));

    if (bvh_is_leaf(node) || bvh_size(node) == 0) {
      // Iterate the node's primitives
      for (uint i = 0; i < bvh_size(node); ++i) {
        // Obtain and unpack next prim; then trace against it
        TLASPrimitive prim = unpack_tlas_primitive(scene_tlas_prim(bvh_offs(node) + i));
        if (prim.is_object) {
          if (ray_intersect_object_any(ray_world, prim.object_i))
            return true;
        } else {
          if (ray_intersect_emitter_any(ray_world, prim.object_i))
            return true;
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
        scene_set_tlas_stack_value(stckc - 1, (mask << bvh_stck_offset) | bvh_offs(node));
      }
    }
  } // while (stckc > 0)

  return false;
}

#endif // RENDER_TLAS_GLSL_GUARD