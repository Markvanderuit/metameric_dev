#ifndef RENDER_BVH_GLSL_GUARD
#define RENDER_BVH_GLSL_GUARD

#include <render/ray.glsl>
#include <render/shape/aabb.glsl>
#include <render/shape/primitive.glsl>

// Hardcoded constants
#define blas_stck_init_value 0x01000000
#define blas_stck_offset     24

// BVH node data queries; operate on BVHNode::data
uint node_get_offs(in uint node) { return bitfieldExtract(node, 0, 19);       }
uint node_get_size(in uint node) { return bitfieldExtract(node, 19, 4);       }
uint node_get_mask(in uint node) { return bitfieldExtract(node, 23, 8);       }
bool node_is_leaf(in uint node)  { return bool(bitfieldExtract(node, 31, 1)); }

// Unpack and intersect <= 8 child node AABBs
uint ray_intersect_aabb_mask(in Ray ray, in BVHNodePack pack) {
  // Current node AABB surrounding child AABBs
  AABB parent = AABB(vec3(unpackUnorm2x16(pack.aabb[0]), unpackUnorm2x16(pack.aabb[2]).x),
                     vec3(unpackUnorm2x16(pack.aabb[1]), unpackUnorm2x16(pack.aabb[2]).y));
  
  // Inverse of ray direction
  vec3 d_rcp = 1.f / ray.d;

  // Return value; bitmask where bits set which of <= 8 child nodes were hit
  uint aabb_mask = 0u;

  // Iterate children
  for (uint child_i = 0; child_i < node_get_size(pack.data); child_i++) {
    vec4 unpack_0 = unpackUnorm4x8(pack.child_aabb_0[child_i]);
    vec4 unpack_1 = unpackUnorm4x8(pack.child_aabb_1[child_i / 2] >> (bool(child_i % 2) ? 16 : 0));

    AABB child = AABB(fma(vec3(unpack_0.xy, unpack_1.x), parent.maxb, parent.minb),
                      fma(vec3(unpack_0.zw, unpack_1.y), parent.maxb, parent.minb));

    vec3 t_max = (child.maxb - ray.o) * d_rcp,
         t_min = (child.minb - ray.o) * d_rcp;

    if (d_rcp.x < 0.f) swap(t_min.x, t_max.x);
    if (d_rcp.y < 0.f) swap(t_min.y, t_max.y);
    if (d_rcp.z < 0.f) swap(t_min.z, t_max.z);

    float t_in  = hmax(t_min), t_out = hmin(t_max);

    // On closest hit (compared to initial ray distance), set child's bit in hit mask
    if (t_out >= 0.f && t_in <= min(ray.t, t_out))
      aabb_mask |= 1u << child_i;
  }

  return aabb_mask;
}

bool ray_intersect_blas(inout Ray ray, in uint mesh_i) {
  // Initiate small stack for traversal from root node
  // Stack values use 8 bits to flag nodes of interest, 
  // and 24 bits to store the offset to these nodes;
#ifndef blas_stck
  uint blas_stck[5];
#endif
  blas_stck[0] = blas_stck_init_value;
  
  // Smaller leaf stack to improve primitive intersection coherency
#ifndef leaf_stck
  uint leaf_stck;
#endif
  leaf_stck = 0u;

  // Return value
  bool hit = false;

  // Continue traversal until stack is once again empty
  int stckc = 0; 
  while (stckc >= 0 || leaf_stck != 0u) {
    // First, handle all entries on leaf stack until empty
    while (leaf_stck != 0) {
      // Read next flagged bit and offset from stack, then remove flagged bit
      int leaf_bit = findMSB(leaf_stck >> blas_stck_offset);
      leaf_stck = bitfieldInsert(leaf_stck, 0u, blas_stck_offset + leaf_bit, 1);

      // Work out index of underlying primitive from node data
      uint leaf_i = scene_blas_info(mesh_i).nodes_offs + (leaf_stck & 0x00FFFFFF) + leaf_bit;
      uint prim_i = scene_blas_info(mesh_i).prims_offs + node_get_offs(scene_blas_node(leaf_i).data);

      // Obtain next prim, then test against it; store primitive index on a hit
      if (ray_intersect(ray, unpack_triangle(scene_blas_prim(prim_i)))) {
        record_set_object_primitive(ray.data, prim_i);
        hit = true;
      }
      
      // If this was the last flagged bit, clear leaf stack
      if ((leaf_stck & 0xFF000000) == 0)
        leaf_stck = 0;
    }

    // Then, handle next entry on node stack
    if (stckc >= 0) {
      // Read next flagged node bit and offset from stack, then remove flagged bit
      int node_bit = findMSB(blas_stck[stckc] >> blas_stck_offset);
      blas_stck[stckc] = bitfieldInsert(blas_stck[stckc], 0u, blas_stck_offset + node_bit, 1);

      // Obtain next node parent data
      uint node_i = scene_blas_info(mesh_i).nodes_offs + (blas_stck[stckc] & 0x00FFFFFF) + node_bit;
      uint node   = scene_blas_node(node_i).data;

      // If this was the last flagged bit, decrease stack count
      if ((blas_stck[stckc] & 0xFF000000) == 0)
        stckc--;

      // Intersect against node children, build intersection mask
      uint aabb_mask = ray_intersect_aabb_mask(ray, scene_blas_node(node_i));
      if (aabb_mask != 0u) {
        // If any children were hit, push the child offset + mask on the stack;
        // we separate and push the mask into node and leaf stacks
        uint node_mask = (aabb_mask & ~node_get_mask(node)), 
             leaf_mask = (aabb_mask & node_get_mask(node));
        if (node_mask != 0)
          blas_stck[++stckc] = (node_mask << blas_stck_offset) | node_get_offs(node);
        if (leaf_mask != 0)
          leaf_stck = (leaf_mask << blas_stck_offset) | node_get_offs(node);
      }
    }
  } // while (...)

  return hit;
}

bool ray_intersect_blas_any(in Ray ray, in uint mesh_i) {
  // Initiate small stack for traversal from root node
  // Stack values use 8 bits to flag nodes of interest, 
  // and 24 bits to store the offset to these nodes;
#ifndef blas_stck
  uint blas_stck[5];
#endif
  blas_stck[0] = blas_stck_init_value;
  
  // Smaller leaf stack to improve primitive intersection coherency
#ifndef leaf_stck
  uint leaf_stck;
#endif
  leaf_stck = 0u;

  // Continue traversal until stack is once again empty
  int stckc = 0; 
  while (stckc >= 0 || leaf_stck != 0u) {
    // First, handle all entries on leaf stack until empty
    while (leaf_stck != 0) {
      // Read next flagged bit and offset from stack, then remove flagged bit
      int leaf_bit = findMSB(leaf_stck >> blas_stck_offset);
      leaf_stck = bitfieldInsert(leaf_stck, 0u, blas_stck_offset + leaf_bit, 1);

      // Work out index of underlying primitive from node data
      uint leaf_i = scene_blas_info(mesh_i).nodes_offs + (leaf_stck & 0x00FFFFFF) + leaf_bit;
      uint prim_i = scene_blas_info(mesh_i).prims_offs + node_get_offs(scene_blas_node(leaf_i).data);

      // Obtain next prim, then test against it; exit on hit
      if (ray_intersect(ray, unpack_triangle(scene_blas_prim(prim_i))))
        return true;
      
      // If this was the last flagged bit, clear leaf stack
      if ((leaf_stck & 0xFF000000) == 0)
        leaf_stck = 0;
    }

    // Then, handle next entry on node stack
    if (stckc >= 0) {
      // Read next flagged node bit and offset from stack, then remove flagged bit
      int node_bit = findMSB(blas_stck[stckc] >> blas_stck_offset);
      blas_stck[stckc] = bitfieldInsert(blas_stck[stckc], 0u, blas_stck_offset + node_bit, 1);

      // Obtain next node parent data
      uint node_i = scene_blas_info(mesh_i).nodes_offs + (blas_stck[stckc] & 0x00FFFFFF) + node_bit;
      uint node   = scene_blas_node(node_i).data;

      // If this was the last flagged bit, decrease stack count
      if ((blas_stck[stckc] & 0xFF000000) == 0)
        stckc--;

      // Intersect against node children, build intersection mask
      uint aabb_mask = ray_intersect_aabb_mask(ray, scene_blas_node(node_i));
      if (aabb_mask != 0u) {
        // If any children were hit, push the child offset + mask on the stack;
        // we separate and push the mask into node and leaf stacks
        uint node_mask = (aabb_mask & ~node_get_mask(node)), 
             leaf_mask = (aabb_mask & node_get_mask(node));
        if (node_mask != 0)
          blas_stck[++stckc] = (node_mask << blas_stck_offset) | node_get_offs(node);
        if (leaf_mask != 0)
          leaf_stck = (leaf_mask << blas_stck_offset) | node_get_offs(node);
      }
    }
  } // while (...)

  // No hit
  return false;
}

#endif // RENDER_BVH_GLSL_GUARD