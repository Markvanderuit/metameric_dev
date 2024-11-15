#ifndef RENDER_TLAS_GLSL_GUARD
#define RENDER_TLAS_GLSL_GUARD

#include <render/blas.glsl>
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

bool ray_intersect_tlas(inout Ray ray_world) {
  // Initiate small stack for traversal from root node
  // Stack values use 8 bits to flag nodes of interest, 
  // and 24 bits to store the offset to these nodes;
#ifndef tlas_stck
  uint tlas_stck[3];
#endif
  tlas_stck[0] = blas_stck_init_value;
  
  // Obtain TLAS information, then generate TLAS-local ray
  // TODO; can we do this at ray generation for the entire renderer?
  Ray ray = ray_transform(ray_world, scene_tlas_info().trf);

  // Continue traversal until stack is once again empty
  int stckc = 0; 
  while (stckc >= 0) {
    // Read next flagged bit and offset from stack, then remove flagged bit
    int  node_bit  = findMSB(tlas_stck[stckc] >> blas_stck_offset);
    uint node_offs = tlas_stck[stckc] & 0x00FFFFFF;
    tlas_stck[stckc] &= (~(1u << (blas_stck_offset + node_bit)));

    // If this was the last flagged bit, decrease stack count
    if ((tlas_stck[stckc] & 0xFF000000) == 0)
      stckc--;

    // Obtain and unpack next node
    uint node_i       = node_offs + node_bit;
    BVHNode0Pack node = scene_tlas_node0(node_i);

    if (node_get_size(node) == 0) {
      continue;
    } else if (node_is_leaf(node)) {
      // Iterate the node's primitives
      uint prim_begin = node_get_offs(node);
      for (uint prim_i = prim_begin; 
                prim_i < prim_begin + node_get_size(node);
              ++prim_i) {
        // Obtain and unpack next prim, then test against object or emitter
        // Copy back closest-hit information on hit
        TLASPrimitive prim = unpack_tlas_primitive(scene_tlas_prim(prim_i));
        if (prim.is_object) {
          if (ray_intersect_object(ray_world, prim.object_i)) {
            ray_transform_inplace(ray, ray_world, scene_tlas_info().trf);
          }
        } else {
          if (ray_intersect_emitter(ray_world, prim.object_i)) {
            ray_transform_inplace(ray, ray_world, scene_tlas_info().trf);
          }
        }
      }
    } else {
      // Load packed node child data
      AABB         parent_aabb = unpack_parent_aabb(node);
      BVHNode1Pack children    = scene_tlas_node1(node_i);

      // Bitmask, initialized to all false
      uint mask = 0;
      
      // Iterate the node's children
      for (uint child_i = 0;
                child_i < node_get_size(node);
              ++child_i) {
        // Unpack the next child, then test against it; 
        // on a hit, we flag its index in the bitmask
        AABB child_aabb = unpack_child_aabb(parent_aabb, children, child_i);
        if (ray_intersect_any(ray, child_aabb))
          mask |= 1u << child_i;
      } // for (uint i)

      // If any children were flagged in the mask, push the child offset + mask on the stack
      if (mask != 0)
        tlas_stck[++stckc] = (mask << blas_stck_offset) | node_get_offs(node);
    }
  } // while (stckc >= 0)

  return is_valid(ray_world);
}

bool ray_intersect_tlas_any(in Ray ray_world) {
  // Initiate small stack for traversal from root node
  // Stack values use 8 bits to flag nodes of interest, 
  // and 24 bits to store the offset to these nodes;
#ifndef tlas_stck
  uint tlas_stck[3];
#endif
  tlas_stck[0] = blas_stck_init_value;
  
  // Obtain TLAS information, then generate TLAS-local ray
  // TODO; can we do this at ray generation for the entire renderer?
  Ray ray = ray_transform(ray_world, /* inverse */(scene_tlas_info().trf));

  // Continue traversal until stack is once again empty
  int stckc = 0; 
  while (stckc >= 0) {
    // Read next flagged bit and offset from stack, then remove flagged bit
    int  node_bit  = findMSB(tlas_stck[stckc] >> blas_stck_offset);
    uint node_offs = tlas_stck[stckc] & 0x00FFFFFF;
    tlas_stck[stckc] &= (~(1u << (blas_stck_offset + node_bit)));

    // If this was the last flagged bit, decrease stack count
    if ((tlas_stck[stckc] & 0xFF000000) == 0)
      stckc--;

    // Obtain and unpack next node
    uint node_i       = node_offs + node_bit;
    BVHNode0Pack node = scene_tlas_node0(node_i);

    if (node_get_size(node) == 0) {
      continue;
    } else if (node_is_leaf(node)) {
      // Iterate the node's primitives
      uint prim_begin = node_get_offs(node);
      for (uint prim_i = prim_begin; 
                prim_i < prim_begin + node_get_size(node);
              ++prim_i) {
        // Obtain and unpack next prim, then test against object or emiitter
        // Copy back information to world ray on hit
        TLASPrimitive prim = unpack_tlas_primitive(scene_tlas_prim(prim_i));
        if (prim.is_object) {
          if (ray_intersect_object_any(ray_world, prim.object_i))
            return true;
        } else {
          if (ray_intersect_emitter_any(ray_world, prim.object_i))
            return true;
        }
      }
    } else {
      // Load packed node child data
      AABB         parent_aabb = unpack_parent_aabb(node);
      BVHNode1Pack children    = scene_tlas_node1(node_i);

      // Bitmask, initialized to all false
      uint mask = 0;
      
      // Iterate the node's children
      for (uint child_i = 0;
                child_i < node_get_size(node);
              ++child_i) {
        // Unpack the next child, then test against it; 
        // on a hit, we flag its index in the bitmask
        AABB child_aabb = unpack_child_aabb(parent_aabb, children, child_i);
        if (ray_intersect_any(ray, child_aabb))
          mask |= 1u << child_i;
      } // for (uint i)

      // If any children were flagged in the mask, push the child offset + mask on the stack
      if (mask != 0)
        tlas_stck[++stckc] = (mask << blas_stck_offset) | node_get_offs(node);
    }
  } // while (stckc >= 0)

  return false;
}

#endif // RENDER_TLAS_GLSL_GUARD