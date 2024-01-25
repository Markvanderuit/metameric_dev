#ifndef SHAPE_AABB_GLSL_GUARD
#define SHAPE_AABB_GLSL_GUARD

#include <render/ray.glsl>

struct AABB {
  vec3 minb; // Minimum of bounding box
  vec3 maxb; // Maximum of bounding box
};

bool ray_intersect(inout Ray ray, in vec3 d_inv, in AABB aabb) {
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

bool ray_intersect(inout Ray ray, in AABB aabb) {
  return ray_intersect(ray, 1.f / ray.d, aabb);
}

bool ray_intersect_any(in Ray ray, in vec3 d_inv, in AABB aabb) {
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

bool ray_intersect_any(in Ray ray, in AABB aabb) {
  return ray_intersect_any(ray, 1.f / ray.d, aabb);
}

#endif // SHAPE_AABB_GLSL_GUARD