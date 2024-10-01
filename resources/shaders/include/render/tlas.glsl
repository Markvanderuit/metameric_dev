#ifndef RENDER_TLAS_GLSL_GUARD
#define RENDER_TLAS_GLSL_GUARD

#include <render/bvh.glsl>

bool ray_intersect_tlas(in Ray ray) {
  vec3 d_inv = 1.f / ray.d;

  // Initiate stack for traversal from root node
  // Stack values use 8 bits to flag nodes of interest, 
  // and 24 bits to store the offset to these nodes
  scene_set_stack_value(0u, 1u << bvh_stck_offset);
  uint stckc    = 1; 
  bool hit      = false;


  // Continue traversal until stack is once again empty
  while (stckc > 0) {

  } // while (stckc > 0)

  return hit;
}

#endif // RENDER_TLAS_GLSL_GUARD