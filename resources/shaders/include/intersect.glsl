#ifndef INTERSECT_GLSL_GUARD
#define INTERSECT_GLSL_GUARD

#include <ray.glsl>
#include <bvh.glsl>

// This header requires the following defines to point to SSBOs or shared memory
// to work around glsl's lack of ssbo argument passing
// #define isct_n_objects      buff_objc_info.data.length()
// #define isct_stack          s_stack[gl_LocalInvocationID.x]
// #define isct_buff_objc_info s_objc_info
// #define isct_buff_bvhs_info s_bvhs_info
// #define isct_buff_bvhs_node buff_bvhs_node.data
// #define isct_buff_bvhs_prim buff_bvhs_prim.data

// Hardcoded constants
#define bvh_stck_offset 24

bool ray_isct_aabb_any(inout Ray ray, in vec3 d_inv, in AABB aabb) {
  vec3 t_max = (aabb.maxb - ray.o) * d_inv;
  vec3 t_min = (aabb.minb - ray.o) * d_inv;
  
  if (d_inv.x < 0.f) swap(t_min.x, t_max.x);
  if (d_inv.y < 0.f) swap(t_min.y, t_max.y);
  if (d_inv.z < 0.f) swap(t_min.z, t_max.z);

  float t_in = hmax(t_min);
  float t_out = hmin(t_max);

  // Entry/Exit/Ray distance test
  return !(t_in > t_out || t_out < 0.f || t_in > ray.t);
}

bool ray_isct_aabb(inout Ray ray, in vec3 d_inv, in AABB aabb) {
  bvec3 degenerate = equal(ray.d, vec3(0));

  vec3 t_max = mix((aabb.minb - ray.o) * d_inv, vec3(FLT_MAX), degenerate);
  vec3 t_min = mix((aabb.maxb - ray.o) * d_inv, vec3(FLT_MIN), degenerate);

  float t_in  = hmax(min(t_min, t_max));
  float t_out = hmin(max(t_min, t_max));

  if (t_in < 0.f && t_out > 0.f) {
    t_in = t_out;
    t_out = FLT_MAX;
  }

  // Entry/Exit/Ray distance test
  if (t_in > t_out || t_in < 0.f || t_in > ray.t)
    return false;
  
  // Update closest-hit distance before return
  ray.t = t_in;
  return true;
}

bool ray_isct_prim(inout Ray ray, in vec3 a, in vec3 b, in vec3 c) {
  vec3 ab = b - a;
  vec3 bc = c - b;
  vec3 n  = normalize(cross(bc, ab)); // TODO is normalize necessary?

  // Ray/plane distance test
  float t = dot(((a + b + c) / 3.f - ray.o), n) / dot(n, ray.d);
  if (t < 0.f || t > ray.t)
    return false;
  
  // Point-in-triangle test
  vec3 p = ray.o + t * ray.d;
  if ((dot(n, cross(p - a, ab))    < 0.f) ||
      (dot(n, cross(p - b, bc))    < 0.f) ||
      (dot(n, cross(p - c, a - c)) < 0.f))
    return false;

  // Update closest-hit distance before return
  ray.t = t;
  return true;
}

bool ray_isct_bvh_any(in Ray ray, in uint bvh_i) {
  vec3 d_inv = 1.f / ray.d;

  // Initiate stack for traversal from root node
  // Stack values use 8 bits to flag nodes of interest, 
  // and 24 bits to store the offset to these nodes
  isct_stack[0] = 1u << bvh_stck_offset;

  // Traversal values; there is one value on the stack
  uint stckc = 1;

  // Continue traversal until stack is once again empty
  while (stckc > 0) {
    // Read offset and bitmask from stack
    uint stck_value = isct_stack[stckc - 1];
    uint node_first = stck_value & 0x00FFFFFF;
    int  node_bit   = findMSB(stck_value >> bvh_stck_offset);

    // Remove bit from mask on stack
    stck_value &= (~(1u << (bvh_stck_offset + node_bit)));
    isct_stack[stckc - 1] = stck_value;

    // If this was the last flagged bit, decrease stack count
    if ((stck_value & 0xFF000000) == 0)
      stckc--;
    
    // Index of next node in buffer
    uint node_i = isct_buff_bvhs_info[bvh_i].nodes_offs 
                + node_first + node_bit;

    // Obtain and unpack next node
    BVHNode node = unpack(isct_buff_bvhs_node[node_i]);

    if (bvh_is_leaf(node) || bvh_size(node) == 0) {
      // Iterate the node's primitives
      for (uint i = 0; i < bvh_size(node); ++i) {
        // Index of next prim in buffer
        uint prim_i = isct_buff_bvhs_info[bvh_i].prims_offs 
                    + bvh_offs(node) + i;

        // Obtain and unpack next prim
        BVHPrim prim = unpack(isct_buff_bvhs_prim[prim_i]);

        // Test against primitive; store primitive index on hit
        if (ray_isct_prim(ray, prim.v0.p, prim.v1.p, prim.v2.p)) {
          return true;
        }
      }
    } else {
      // Iterate the node's children, test against each AABB, and
      // on a hit flag the index of the child in a bitmask
      uint mask = 0;
      for (uint i = 0; i < bvh_size(node); ++i) {
        if (ray_isct_aabb_any(ray, d_inv, bvh_child_aabb(node, i)))
          mask |= (1u << i);
      } // for (uint i)

      // If any children were hit, indicated by bitflips, push the child offset + mask on the stack
      if (mask != 0) {
        stckc++;
        isct_stack[stckc - 1] = (mask << bvh_stck_offset) | bvh_offs(node);
      }
    }
  } // while (stckc > 0)

  return false;
}

bool ray_isct_bvh(inout Ray ray, in uint bvh_i) {
  vec3 d_inv = 1.f / ray.d;

  // Initiate stack for traversal from root node
  // Stack values use 8 bits to flag nodes of interest, 
  // and 24 bits to store the offset to these nodes
  isct_stack[0] = 1u << bvh_stck_offset;

  // Traversal values; there is one value on the stack
  uint stckc = 1; 
  bool hit    = false;

  // Continue traversal until stack is once again empty
  while (stckc > 0) {
    // Read offset and bitmask from stack
    uint stck_value = isct_stack[stckc - 1];
    uint node_first = stck_value & 0x00FFFFFF;
    int  node_bit   = findMSB(stck_value >> bvh_stck_offset);

    // Remove bit from mask on stack
    stck_value &= (~(1u << (bvh_stck_offset + node_bit)));
    isct_stack[stckc - 1] = stck_value;

    // If this was the last flagged bit, decrease stack count
    if ((stck_value & 0xFF000000) == 0)
      stckc--;
    
    // Index of next node in buffer
    uint node_i = isct_buff_bvhs_info[bvh_i].nodes_offs 
                + node_first + node_bit;

    // Obtain and unpack next node
    BVHNode node = unpack(isct_buff_bvhs_node[node_i]);

    if (bvh_is_leaf(node) || bvh_size(node) == 0) {
      // Iterate the node's primitives
      for (uint i = 0; i < bvh_size(node); ++i) {
        // Index of next prim in buffer
        uint prim_i = isct_buff_bvhs_info[bvh_i].prims_offs 
                    + bvh_offs(node) + i;

        // Obtain and unpack next prim
        BVHPrim prim = unpack(isct_buff_bvhs_prim[prim_i]);

        // Test against primitive; store primitive index on hit
        if (ray_isct_prim(ray, prim.v0.p, prim.v1.p, prim.v2.p)) {
          set_ray_data_prim(ray, prim_i);
          hit       = true;
        }
      }
    } else {
      // Iterate the node's children, test against each AABB, and
      // on a hit flag the index of the child in a bitmask
      uint mask = 0;
      for (uint i = 0; i < bvh_size(node); ++i) {
        if (ray_isct_aabb_any(ray, d_inv, bvh_child_aabb(node, i)))
          mask |= (1u << i);
      } // for (uint i)

      // If any children were hit, indicated by bitflips, push the child offset + mask on the stack
      if (mask != 0) {
        stckc++;
        isct_stack[stckc - 1] = (mask << bvh_stck_offset) | bvh_offs(node);
      }
    }
  } // while (stckc > 0)

  return hit;
}

bool ray_isct_object_any(in Ray ray, uint object_i) {
  ObjectInfo object_info = s_objc_info[object_i];
  BVHInfo    bvh_info    = s_bvhs_info[object_info.mesh_i];
  
  if (!object_info.is_active)
    return false;

  // TODO streamline this stuff
  // Setup transformation to take world space ray into local space
  mat4 trf = object_info.trf * bvh_info.trf;
  mat4 inv = inverse(trf);
  
  // Generate object space ray
  Ray ray_object;
  ray_object.o = (inv * vec4(ray.o, 1)).xyz;
  ray_object.d = (inv * vec4(ray.d, 0)).xyz;
  
  // Get length and normalize direction
  // Reuse length to adjust ray_object.t if ray.t is not at infty
  float dt = length(ray_object.d);
  ray_object.d /= dt;
  ray_object.t = (ray.t == FLT_MAX) ? FLT_MAX : dt * ray.t;
  
  return ray_isct_bvh_any(ray_object, object_info.mesh_i);
}

void ray_isct_object(inout Ray ray, uint object_i) {
  ObjectInfo object_info = s_objc_info[object_i];
  BVHInfo    bvh_info    = s_bvhs_info[object_info.mesh_i];

  if (!object_info.is_active)
    return;
  
  // TODO streamline this stuff
  // Setup transformation to take world space ray into local space
  mat4 trf = object_info.trf * bvh_info.trf;
  mat4 inv = inverse(trf);

  // Generate object space ray
  Ray ray_object;
  ray_object.o = (inv * vec4(ray.o, 1)).xyz;
  ray_object.d = (inv * vec4(ray.d, 0)).xyz;
  
  // Get length and normalize direction
  // Reuse length to adjust ray_object.t if ray.t is not at infty
  float dt = length(ray_object.d);
  ray_object.d /= dt;
  ray_object.t = (ray.t == FLT_MAX) ? FLT_MAX : dt * ray.t;

  // Run intersection; on a hit, recover world-space distance,
  // and store intersection data in ray
  if (ray_isct_bvh(ray_object, object_info.mesh_i)) {
    ray.t    = length((trf * vec4(ray_object.d * ray_object.t, 0)).xyz);
    ray.data = ray_object.data;
    set_ray_data_objc(ray, object_i);
  }
}

bool ray_intersect_scene_any(inout Ray ray) {
  set_ray_data_anyh(ray, false);
  for (uint i = 0; i < isct_n_objects; ++i) {
    if (ray_isct_object_any(ray, i)) {
      set_ray_data_anyh(ray, true);
      return true;
    }
  }
  return false;
}

bool ray_intersect_scene(inout Ray ray) {
  set_ray_data_objc(ray, OBJECT_INVALID);
  for (uint i = 0; i < isct_n_objects; ++i) {
    ray_isct_object(ray, i);
  }
  return get_ray_data_objc(ray) != OBJECT_INVALID;
}

bool ray_intersect_any(inout Ray ray) {
  return ray_intersect_scene_any(ray);
}

/* SurfaceInfo ray_intersect(in Ray ray) {
  // First, handle ray-scene intersection
  ray_intersect_scene(ray);

  SurfaceInfo si;
  si.object_i = get_ray_data_objc(ray);

  // On a hit, generate surface info
  if (si.object_i != OBJECT_INVALID) {
    // Obtain and unpack intersected primitive
    BVHPrim prim = unpack(isct_buff_bvhs_prim[get_ray_data_prim(ray)]);

    // Fill in surface point from intersection
    si.t = ray.t;
    si.p = ray.o + ray.d * ray.t;

    // Fill rest of surface info object
    vec3 b = gen_barycentric_coords(si.p, prim.v0.p, prim.v1.p, prim.v2.p);
    si.tx  = b.x * prim.v0.tx + b.y * prim.v1.tx + b.z * prim.v2.tx;
    si.n   = b.x * prim.v0.n  + b.y * prim.v1.n  + b.z * prim.v2.n;
    si.n   = normalize(si.n); // Why necessary? Grug find bug!
  }

  return si;
} */

#endif // INTERSECT_GLSL_GUARD