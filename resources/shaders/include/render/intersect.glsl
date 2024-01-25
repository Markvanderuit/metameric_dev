#ifndef INTERSECT_GLSL_GUARD
#define INTERSECT_GLSL_GUARD

#include <render/ray.glsl>
#include <render/scene.glsl>
#include <render/shape/aabb.glsl>
#include <render/shape/primitive.glsl>
#include <render/shape/rectangle.glsl>
#include <render/shape/sphere.glsl>

// Hardcoded constants
#define bvh_stck_offset 24

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
    
    // Index of next node in buffer
    uint node_i = mesh_info.nodes_offs + node_first + node_bit;

    // Obtain and unpack next node
    BVHNode node = unpack(scene_mesh_node(node_i));

    if (bvh_is_leaf(node) || bvh_size(node) == 0) {
      // Iterate the node's primitives
      for (uint i = 0; i < bvh_size(node); ++i) {
        // Index of next prim in buffer
        uint prim_i = mesh_info.prims_offs + bvh_offs(node) + i;

        // Obtain and unpack next prim
        Primitive prim = unpack_primitive(scene_mesh_prim(prim_i));

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
        AABB aabb = bvh_child_aabb(node, i);
        if (ray_intersect_any(ray, d_inv, aabb))
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

bool ray_intersect_bvh(inout Ray ray, in uint bvh_i) {
  vec3 d_inv = 1.f / ray.d;

  MeshInfo mesh_info = scene_mesh_info(bvh_i);

  // Initiate stack for traversal from root node
  // Stack values use 8 bits to flag nodes of interest, 
  // and 24 bits to store the offset to these nodes
  scene_set_stack_value(0u, 1u << bvh_stck_offset);
  uint stckc    = 1; 
  bool hit      = false;

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
    
    // Index of next node in buffer
    uint node_i = mesh_info.nodes_offs + node_first + node_bit;

    // Obtain and unpack next node
    BVHNode node = unpack(scene_mesh_node(node_i));

    if (bvh_is_leaf(node) || bvh_size(node) == 0) {
      // Iterate the node's primitives
      for (uint i = 0; i < bvh_size(node); ++i) {
        // Index of next prim in buffer
        uint prim_i = mesh_info.prims_offs + bvh_offs(node) + i;

        // Obtain and unpack next prim
        Primitive prim = unpack_primitive(scene_mesh_prim(prim_i));

        // Test against primitive; store primitive index on hit
        if (ray_intersect(ray, prim)) {
          record_set_object_primitive(ray.data, bvh_offs(node) + i);
          hit = true;
        }
      }
    } else {
      // Iterate the node's children, test against each AABB, and
      // on a hit flag the index of the child in a bitmask
      uint mask = 0;
      for (uint i = 0; i < bvh_size(node); ++i) {
        AABB aabb = bvh_child_aabb(node, i);
        if (ray_intersect_any(ray, d_inv, aabb))
          mask |= (1u << i);
      } // for (uint i)

      // If any children were hit, indicated by bitflips, push the child offset + mask on the stack
      if (mask != 0) {
        stckc++;
        scene_set_stack_value(stckc - 1, (mask << bvh_stck_offset) | bvh_offs(node));
      }
    }
  } // while (stckc > 0)

  return hit;
}

bool ray_intersect_object_any(in Ray ray, uint object_i) {
  ObjectInfo object_info = scene_object_info(object_i);
  
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
  
  return ray_intersect_bvh_any(ray_local, object_info.mesh_i);
}

void ray_intersect_object(inout Ray ray, uint object_i) {
  ObjectInfo object_info = scene_object_info(object_i);
  
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
  if (ray_intersect_bvh(ray_local, object_info.mesh_i)) {
    ray.t    = length((object_info.trf_mesh * vec4(ray_local.d * ray_local.t, 0)).xyz);
    ray.data = ray_local.data;
    record_set_object(ray.data, object_i);
  }
}

bool ray_intersect_emitter_any(in Ray ray, in uint emitter_i) {
  EmitterInfo em = scene_emitter_info(emitter_i);
  
  if (!em.is_active || em.type == EmitterTypeConstant || em.type == EmitterTypePoint)
    return false;
  
  // Run intersection; on a hit, simply return
  if (em.type == EmitterTypeSphere) {
    Sphere sphere = { em.center, em.sphere_r };
    return ray_intersect(ray, sphere);
  } else if (em.type == EmitterTypeRectangle) {
    return ray_intersect(ray, em.center, em.rect_n, em.trf_inv);
  }
}

bool ray_intersect_emitter(inout Ray ray, in uint emitter_i) {
  EmitterInfo em = scene_emitter_info(emitter_i);
  
  if (!em.is_active || em.type == EmitterTypeConstant || em.type == EmitterTypePoint)
    return false;

  bool hit = false;
  if (em.type == EmitterTypeSphere) {
    Sphere sphere = { em.center, em.sphere_r };
    hit = ray_intersect(ray, sphere);
  } else if (em.type == EmitterTypeRectangle) {
    hit = ray_intersect(ray, em.center, em.rect_n, em.trf_inv);
  }

  if (hit)
    record_set_emitter(ray.data, emitter_i);

  return hit;
}

bool scene_intersect(inout Ray ray) {
  for (uint i = 0; i < scene_object_count(); ++i) {
    ray_intersect_object(ray, i);
  }
  for (uint i = 0; i < scene_emitter_count(); ++i) {
    ray_intersect_emitter(ray, i);
  }
  return is_valid(ray);
}

bool scene_intersect_any(in Ray ray) {
  for (uint i = 0; i < scene_object_count(); ++i) {
    if (ray_intersect_object_any(ray, i)) {
      return true;
    }
  }
  for (uint i = 0; i < scene_emitter_count(); ++i) {
    if (ray_intersect_emitter_any(ray, i)) {
      return true;
    }
  }
  return false;
}

#endif // INTERSECT_GLSL_GUARD