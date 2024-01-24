#ifndef INTERSECT_GLSL_GUARD
#define INTERSECT_GLSL_GUARD

#include <ray.glsl>
#include <bvh.glsl>
#include <scene.glsl>

// This header requires the following defines to point to SSBOs or shared memory
// to work around glsl's lack of ssbo argument passing
// #define isct_n_objects      buff_objc_info.n
// #define isct_n_emitters     buff_emtr_info.n
// #define isct_stack          s_stack[gl_LocalInvocationID.x]
// #define isct_buff_objc_info s_objc_info
// #define isct_buff_emtr_info s_emtr_info
// #define isct_buff_bvhs_info s_bvhs_info
// #define isct_buff_bvhs_node buff_bvhs_node.data
// #define isct_buff_bvhs_prim buff_bvhs_prim.data

// Hardcoded constants
#define bvh_stck_offset 24

bool ray_intersect_unit_rect(inout Ray ray) {
  // Plane distance test
  float t = -ray.o.z / ray.d.z;
  if (t < 0.f || t > ray.t)
    return false;

  // Plane boundary test
  vec3 p = ray.o + ray.d * t;
  if (any(greaterThan(abs(p.xy), vec2(1))))
    return false;
  
  ray.t = t;
  return true;
}

bool ray_intersect_rect(inout Ray ray, in vec3 c, in vec3 n, in mat4 trf_inv) {
  // Plane distance test
  float t = (dot(c, n) - dot(ray.o, n)) / dot(ray.d, n);
  if (t < 0.f || t > ray.t)
    return false;

  // Plane boundary test, clamp to rectangle of size 1 with (0, 0) at its center
  vec2 p_local = (trf_inv * vec4(ray.o + ray.d * t, 1)).xy;
  if (clamp(p_local, vec2(-.5), vec2(.5)) != p_local)
    return false;
    
  ray.t = t;
  return true;
}

bool ray_intersect_sphere(inout Ray ray, in vec3 center, in float r) {
  vec3  o = ray.o - center;
  float b = 2.f * dot(o, ray.d);
  float c = sdot(o) - sdot(r);
  float d = b * b - 4.f * c;

  float t_near, t_far;

  if (d < 0) {
    return false;
  } else if (d == 0.f) {
    t_near = t_far = -b * 0.5f;
  } else {
    d = sqrt(d);
    t_near = (-b + d) * 0.5f;
    t_far  = (-b - d) * 0.5f;
  }

  if (t_near < 0.f)
    t_near = FLT_MAX;
  if (t_far < 0.f)
    t_far = FLT_MAX;
  
  float t = min(t_near, t_far);
  if (t > ray.t || t < 0.f)
    return false;

  ray.t = t;
  return true;
}

bool ray_intersect_unit_sphere(inout Ray ray) {
  float b = dot(ray.o, ray.d) * 2.f;
  float c = sdot(ray.o) - 1.f;

  float discrim = b * b - 4.f * c;
  if (discrim < 0)
    return false;
  
  float t_near = -.5f * (b + sqrt(discrim) * (b >= 0 ? 1.f : -1.f));
  float t_far  = c / t_near;

  if (t_near > t_far)
    swap(t_near, t_far);

  if (t_far < 0.f || t_near > ray.t)
    return false;

  if (t_far > ray.t && t_near < 0.f)
    return false;

  ray.t = t_near;
  return true;
}

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
  vec3 n  = normalize(cross(bc, ab));
  
  // Backface test
  float cos_theta = dot(n, ray.d);
  /* if (cos_theta <= 0)
    return false; */

  // Ray/plane distance test
  float t = dot(((a + b + c) / 3.f - ray.o), n) / cos_theta;
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

  MeshInfo mesh_info = isct_buff_bvhs_info[bvh_i];

  // Initiate stack for traversal from root node
  // Stack values use 8 bits to flag nodes of interest, 
  // and 24 bits to store the offset to these nodes
  isct_stack[0] = 1u << bvh_stck_offset;
  uint stckc    = 1;

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
    uint node_i = mesh_info.nodes_offs + node_first + node_bit;

    // Obtain and unpack next node
    BVHNode node = unpack(isct_buff_bvhs_node[node_i]);

    if (bvh_is_leaf(node) || bvh_size(node) == 0) {
      // Iterate the node's primitives
      for (uint i = 0; i < bvh_size(node); ++i) {
        // Index of next prim in buffer
        uint prim_i = mesh_info.prims_offs + bvh_offs(node) + i;

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

  MeshInfo mesh_info = isct_buff_bvhs_info[bvh_i];

  // Initiate stack for traversal from root node
  // Stack values use 8 bits to flag nodes of interest, 
  // and 24 bits to store the offset to these nodes
  isct_stack[0] = 1u << bvh_stck_offset;
  uint stckc    = 1; 
  bool hit      = false;

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
    uint node_i = mesh_info.nodes_offs + node_first + node_bit;

    // Obtain and unpack next node
    BVHNode node = unpack(isct_buff_bvhs_node[node_i]);

    if (bvh_is_leaf(node) || bvh_size(node) == 0) {
      // Iterate the node's primitives
      for (uint i = 0; i < bvh_size(node); ++i) {
        // Index of next prim in buffer
        uint prim_i = mesh_info.prims_offs + bvh_offs(node) + i;

        // Obtain and unpack next prim
        BVHPrim prim = unpack(isct_buff_bvhs_prim[prim_i]);

        // Test against primitive; store primitive index on hit
        if (ray_isct_prim(ray, prim.v0.p, prim.v1.p, prim.v2.p)) {
          record_set_object_primitive(ray.data, bvh_offs(node) + i);
          hit = true;
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

bool ray_intersect_object_any(in Ray ray, uint object_i) {
  ObjectInfo object_info = isct_buff_objc_info[object_i];
  
  if (!object_info.is_active)
    return false;
  
  // Generate object space ray
  Ray ray_local;
  ray_local.o = (object_info.trf_mesh_inv * vec4(ray.o, 1)).xyz;
  ray_local.d = (object_info.trf_mesh_inv * vec4(ray.d, 0)).xyz;
  
  // Get length and normalize direction
  // Reuse length to adjust ray_local.t if ray.t is not at infty
  float dt = length(ray_local.d);
  ray_local.d /= dt;
  ray_local.t = (ray.t == FLT_MAX) ? FLT_MAX : dt * ray.t;
  
  return ray_isct_bvh_any(ray_local, object_info.mesh_i);
}

void ray_intersect_object(inout Ray ray, uint object_i) {
  ObjectInfo object_info = isct_buff_objc_info[object_i];
  
  if (!object_info.is_active)
    return;

  // Generate local ray
  Ray ray_local;
  ray_local.o = (object_info.trf_mesh_inv * vec4(ray.o, 1)).xyz;
  ray_local.d = (object_info.trf_mesh_inv * vec4(ray.d, 0)).xyz;
  
  // Get length and normalize direction
  // Reuse length to adjust ray_local.t if ray.t is not at infty
  float dt = length(ray_local.d);
  ray_local.d /= dt;
  ray_local.t = (ray.t == FLT_MAX) ? FLT_MAX : dt * ray.t;

  // Run intersection; on a hit, recover world-space distance,
  // and store intersection data in ray
  if (ray_isct_bvh(ray_local, object_info.mesh_i)) {
    ray.t    = length((object_info.trf_mesh * vec4(ray_local.d * ray_local.t, 0)).xyz);
    ray.data = ray_local.data;
    record_set_object(ray.data, object_i);
  }
}

bool ray_intersect_emitter_any(in Ray ray, in uint emitter_i) {
  EmitterInfo em = isct_buff_emtr_info[emitter_i];
  
  if (!em.is_active || em.type == EmitterTypeConstant || em.type == EmitterTypePoint)
    return false;
  
  // Run intersection; on a hit, simply return
  if (em.type == EmitterTypeSphere) {
    return ray_intersect_sphere(ray, em.center, em.sphere_r);
  } else if (em.type == EmitterTypeRect) {
    return ray_intersect_rect(ray, em.center, em.rect_n, em.trf_inv);
  }
}

bool ray_intersect_emitter(inout Ray ray, in uint emitter_i) {
  EmitterInfo em = isct_buff_emtr_info[emitter_i];
  
  if (!em.is_active || em.type == EmitterTypeConstant || em.type == EmitterTypePoint)
    return false;

  bool hit = false;
  if (em.type == EmitterTypeSphere) {
    hit = ray_intersect_sphere(ray, em.center, em.sphere_r);
  } else if (em.type == EmitterTypeRect) {
    hit = ray_intersect_rect(ray, em.center, em.rect_n, em.trf_inv);
  }

  if (hit)
    record_set_emitter(ray.data, emitter_i);

  return hit;
}

bool ray_intersect_scene_any(in Ray ray) {
  for (uint i = 0; i < isct_n_objects; ++i) {
    if (ray_intersect_object_any(ray, i)) {
      return true;
    }
  }
  
  for (uint i = 0; i < isct_n_emitters; ++i) {
    if (ray_intersect_emitter_any(ray, i)) {
      return true;
    }
  }
  return false;
}

bool ray_intersect_scene(inout Ray ray) {
  for (uint i = 0; i < isct_n_objects; ++i) {
    ray_intersect_object(ray, i);
  }
  
  for (uint i = 0; i < isct_n_emitters; ++i) {
    ray_intersect_emitter(ray, i);
  }

  return is_valid(ray);
}

bool ray_intersect(inout Ray ray) {
  return ray_intersect_scene(ray);
}

bool ray_intersect_any(in Ray ray) {
  return ray_intersect_scene_any(ray);
}

#endif // INTERSECT_GLSL_GUARD