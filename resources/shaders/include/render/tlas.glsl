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

// Query only the data layout of a specific node
uint get_tlas_node_data(uint i) {
  return scene_tlas_node(i).data;
}

bool ray_intersect_tlas(inout Ray ray_world) {
  // Initiate small stack for traversal from root node
  // Stack values use 8 bits to flag nodes of interest, 
  // and 24 bits to store the offset to these nodes;
#ifndef tlas_stck
  uint tlas_stck[5];
#endif
  tlas_stck[0] = blas_stck_init_value;
  
  // Smaller leaf stack to improve primitive intersection coherency
  uint leaf_stck = 0u;
  
  // Obtain TLAS information, then generate TLAS-local ray
  // TODO; can we do this at ray generation for the entire renderer?
  Ray ray = ray_transform(ray_world, scene_tlas_info().trf);

  // Continue traversal until stack is once again empty
  int stckc = 0; 
  while (stckc >= 0 || leaf_stck != 0u) {
    // First, handle all entries on leaf stack until empty
    while (leaf_stck != 0) {
      // Read next flagged bit and offset from stack, then remove flagged bit
      int leaf_bit = findMSB(leaf_stck >> blas_stck_offset);
      leaf_stck = bitfieldInsert(leaf_stck, 0u, blas_stck_offset + leaf_bit, 1);

      // Work out index of underlying primitive from node data
      uint leaf_i = (leaf_stck & 0x00FFFFFF) + leaf_bit;
      uint prim_i = node_get_offs(get_tlas_node_data(leaf_i));

      // Obtain next prim, then test against it; copy back ray hit information on hit
      TLASPrimitive prim = unpack_tlas_primitive(scene_tlas_prim(prim_i));
      if (prim.is_object) {
        if (ray_intersect_object(ray_world, prim.object_i))
          ray_transform_inplace(ray, ray_world, scene_tlas_info().trf);
      } else {
        if (ray_intersect_emitter(ray_world, prim.object_i))
          ray_transform_inplace(ray, ray_world, scene_tlas_info().trf);
      }
      
      // If this was the last flagged bit, clear leaf stack
      if ((leaf_stck & 0xFF000000) == 0)
        leaf_stck = 0;
    }

    if (stckc >= 0) {
      // Read next flagged node bit and offset from stack, then remove flagged bit
      int node_bit = findMSB(tlas_stck[stckc] >> blas_stck_offset);
      tlas_stck[stckc] = bitfieldInsert(tlas_stck[stckc], 0u, blas_stck_offset + node_bit, 1);

      // Obtain next node parent data
      uint node_i = (tlas_stck[stckc] & 0x00FFFFFF) + node_bit;
      uint node   = get_tlas_node_data(node_i);

      // If this was the last flagged bit, decrease stack count
      if ((tlas_stck[stckc] & 0xFF000000) == 0)
        stckc--;

      // Intersect against node children, build intersection mask
      uint aabb_mask = ray_intersect_aabb_mask(ray, scene_tlas_node(node_i));
      if (aabb_mask != 0u) {
        // If any children were hit, push the child offset + mask on the stack;
        // we separate and push the mask into node and leaf stacks
        uint node_mask = (aabb_mask & ~node_get_mask(node)), 
             leaf_mask = (aabb_mask & node_get_mask(node));
        if (node_mask != 0)
          tlas_stck[++stckc] = (node_mask << blas_stck_offset) | node_get_offs(node);
        if (leaf_mask != 0)
          leaf_stck = (leaf_mask << blas_stck_offset) | node_get_offs(node);
      }
    }
  } // while (...)

  return is_valid(ray_world);
}

bool ray_intersect_tlas_any(in Ray ray_world) {
  // Initiate small stack for traversal from root node
  // Stack values use 8 bits to flag nodes of interest, 
  // and 24 bits to store the offset to these nodes;
#ifndef tlas_stck
  uint tlas_stck[5];
#endif
  tlas_stck[0] = blas_stck_init_value;
  
  // Smaller leaf stack to improve primitive intersection coherency
  uint leaf_stck = 0u;
  
  // Obtain TLAS information, then generate TLAS-local ray
  // TODO; can we do this at ray generation for the entire renderer?
  Ray ray = ray_transform(ray_world, scene_tlas_info().trf);

  // Continue traversal until stack is once again empty
  int stckc = 0; 
  while (stckc >= 0 || leaf_stck != 0u) {
    // First, handle all entries on leaf stack until empty
    while (leaf_stck != 0) {
      // Read next flagged bit and offset from stack, then remove flagged bit
      int leaf_bit = findMSB(leaf_stck >> blas_stck_offset);
      leaf_stck = bitfieldInsert(leaf_stck, 0u, blas_stck_offset + leaf_bit, 1);

      // Work out index of underlying primitive from node data
      uint leaf_i = (leaf_stck & 0x00FFFFFF) + leaf_bit;
      uint prim_i = node_get_offs(get_tlas_node_data(leaf_i));

      // Obtain next prim, then test against it; copy back ray hit information on hit
      TLASPrimitive prim = unpack_tlas_primitive(scene_tlas_prim(prim_i));
      if (prim.is_object) {
        if (ray_intersect_object(ray_world, prim.object_i))
          return true;
      } else {
        if (ray_intersect_emitter(ray_world, prim.object_i))
          return true;
      }
      
      // If this was the last flagged bit, clear leaf stack
      if ((leaf_stck & 0xFF000000) == 0)
        leaf_stck = 0;
    }

    if (stckc >= 0) {
      // Read next flagged node bit and offset from stack, then remove flagged bit
      int node_bit = findMSB(tlas_stck[stckc] >> blas_stck_offset);
      tlas_stck[stckc] = bitfieldInsert(tlas_stck[stckc], 0u, blas_stck_offset + node_bit, 1);

      // Obtain next node parent data
      uint node_i = (tlas_stck[stckc] & 0x00FFFFFF) + node_bit;
      uint node   = get_tlas_node_data(node_i);

      // If this was the last flagged bit, decrease stack count
      if ((tlas_stck[stckc] & 0xFF000000) == 0)
        stckc--;

      // Intersect against node children, build intersection mask
      uint aabb_mask = ray_intersect_aabb_mask(ray, scene_tlas_node(node_i));
      if (aabb_mask != 0u) {
        // If any children were hit, push the child offset + mask on the stack;
        // we separate and push the mask into node and leaf stacks
        uint node_mask = (aabb_mask & ~node_get_mask(node)), 
             leaf_mask = (aabb_mask & node_get_mask(node));
        if (node_mask != 0)
          tlas_stck[++stckc] = (node_mask << blas_stck_offset) | node_get_offs(node);
        if (leaf_mask != 0)
          leaf_stck = (leaf_mask << blas_stck_offset) | node_get_offs(node);
      }
    }
  } // while (...)

  // No hit
  return false;
}

#endif // RENDER_TLAS_GLSL_GUARD