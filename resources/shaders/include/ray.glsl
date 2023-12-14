#ifndef RAY_GLSL_GUARD
#define RAY_GLSL_GUARD

#include <math.glsl>

struct Ray {
  vec3  o;
  vec3  d;
  float t;
};

struct AABB {
  vec3 minb; // Minimum of bounding box
  vec3 maxb; // Maximum of bounding box
};

struct Hit {
  vec3 p;
  vec3 n;
  vec2 tx;
  uint mesh_i;
  uint uplf_i;
};

// Should be 32 bytes
// NOTE: Needs resizing on other side as well
struct RayQuery {
  vec3  o;
  float t;
  vec3  d;
  uint  object_i;
};

// Shorter test; generally returns true if the ray is within the aabb
bool intersect_ray_aabb_fast(in Ray ray, in AABB aabb, inout float t_isct) {
  // Extract if you always need it!
  vec3 inv_ray_d = 1.f / ray.d;

  vec3 t1 = (aabb.minb - ray.o) * inv_ray_d;
  vec3 t2 = (aabb.maxb - ray.o) * inv_ray_d;

  if (inv_ray_d.x < 0.f)
    swap(t1.x, t2.x);
  if (inv_ray_d.y < 0.f)
    swap(t1.y, t2.y);
  if (inv_ray_d.z < 0.f)
    swap(t1.z, t2.z);

  float t_min = hmax(t1);
  float t_max = hmin(t2);

  t_isct = t_min;

  return !(t_min > t_max || t_max < 0.f || t_min > ray.t);
}

bool intersect_ray_aabb(inout Ray ray, in AABB aabb) {
  // Extract if you always need it!
  vec3 inv_ray_d = 1.f / ray.d;
  
	const vec3 f = (aabb.maxb - ray.o) * inv_ray_d;
	const vec3 n = (aabb.minb - ray.o) * inv_ray_d;
	const vec3 t_max = max(f, n);
	const vec3 t_min = min(f, n);

	const float t_out = hmin(t_max); // min(tmax.x, min(tmax.y, tmax.z));
	const float t_in  = max(0.f, hmax(t_min)); // max(max(tmin.x, max(tmin.y, tmin.z)), 0.0f);
  
  // Entry/Exit/Ray distance test
  if (t_in > t_out || t_in < 0.f || t_in > ray.t)
    return false;

  // Update closest-hit distance before return
  ray.t = t_in;
  return true;

  /* vec3 t_min = aabb.minb - ray.o;
  vec3 t_max = aabb.maxb - ray.o;

  bvec3 degenerate = equal(ray.d, vec3(0));
  t_max = mix(t_max * inv_ray_d, vec3(FLT_MAX), degenerate);
  t_min = mix(t_min * inv_ray_d, vec3(FLT_MIN), degenerate);

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
  return true; */
}

bool intersect_ray_prim(inout Ray ray, in vec3 a, in vec3 b, in vec3 c) {
  vec3 ab = b - a;
  vec3 bc = c - b;
  vec3 ca = a - c;
  vec3 n  = normalize(cross(bc, ab)); // TODO is normalize necessary?

  // Backface test
  float n_dot_d = dot(n, ray.d);
  /* if (n_dot_d < 0.f)
    return false; */
  
  // Ray/plane distance test
  float t = dot(((a + b + c) / 3.f - ray.o), n) / n_dot_d;
  if (t < 0.f || t > ray.t)
    return false;
  
  // Point-in-triangle test
  vec3 p = ray.o + t * ray.d;
  if ((dot(n, cross(p - a, ab)) < 0.f) ||
      (dot(n, cross(p - b, bc)) < 0.f) ||
      (dot(n, cross(p - c, ca)) < 0.f))
    return false;

  // Update closest-hit distance before return
  ray.t = t;
  return true;
}

#endif // RAY_GLSL_GUARD