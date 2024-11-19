#ifndef SHAPE_AABB_GLSL_GUARD
#define SHAPE_AABB_GLSL_GUARD

#include <render/ray.glsl>

struct AABB {
  vec3 minb; // Minimum of bounding box
  vec3 maxb; // Maximum of bounding box
};

bool ray_intersect(inout Ray ray, in AABB aabb) {
  if (aabb.minb == aabb.maxb)
    return false;
    
  bvec3 degenerate = equal(ray.d, vec3(0));

  vec3 d_rcp = 1.f / ray.d;
  vec3 t_max = mix((aabb.minb - ray.o) * d_rcp, vec3(FLT_MAX), degenerate);
  vec3 t_min = mix((aabb.maxb - ray.o) * d_rcp, vec3(FLT_MIN), degenerate);
  float t_in = hmax(min(t_min, t_max)), t_out = hmin(max(t_min, t_max));
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

bool ray_intersect_any(in Ray ray, in AABB aabb) {
  if (aabb.minb == aabb.maxb)
    return false;

  vec3 d_rcp = 1.f / ray.d;
  vec3 t_max = (aabb.maxb - ray.o) * d_rcp;
  vec3 t_min = (aabb.minb - ray.o) * d_rcp;
  
  if (d_rcp.x < 0.f) swap(t_min.x, t_max.x);
  if (d_rcp.y < 0.f) swap(t_min.y, t_max.y);
  if (d_rcp.z < 0.f) swap(t_min.z, t_max.z);

  float t_in  = hmax(t_min);
  float t_out = hmin(t_max);

  // Entry/Exit/Ray distance test
  return !(t_in > t_out || t_out < 0.f || t_in > ray.t);
}

#endif // SHAPE_AABB_GLSL_GUARD