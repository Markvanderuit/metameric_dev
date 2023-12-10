#ifndef RAY_GLSL_GUARD
#define RAY_GLSL_GUARD

#include <math.glsl>

struct Ray {
  vec3 o;
  vec3 d;
  float t;
};

struct BBox {
  vec3 minb;
  vec3 maxb;
};

struct Hit {

};

void swap(inout float a, inout float b) {
  float t = a;
  a = b;
  b = t;
}

bool intersect_ray_bbox(in Ray ray, in BBox bbox, inout float t_isct) {
  vec3 inv_ray_d = 1.f / ray.d;

  vec3 t1 = (bbox.minb - ray.o) * inv_ray_d;
  if (inv_ray_d.x < 0.f)
    swap(t1.x, t2.x);
  if (inv_ray_d.y < 0.f)
    swap(t1.y, t2.y);
  if (inv_ray_d.z < 0.f)
    swap(t1.z, t2.z);

  float t_min = hmax(t1);
  float t_max = hmax(t2);

  t_isct = t_min;

  return !(t_min > t_max || t_max < 0.f || t_min > ray.t);
}

bool intersect_ray_prim(inout Ray ray, in vec3 p0, in vec3 p1, in vec3 p2) {
  vec3 ab = b - a;
  vec3 bc = c - b;
  vec3 ca = a - c;

  // Plane normal
  vec3 n  = normalize(bc.cross(ab)); // TODO is normalize necessary?

  // Backface test
  float n_dot_d = dot(n, ray.d);
  if (n_dot_d < 0.f)
    return false;
  
  // Plane distance test
  float t = dot(((a + b + c) / 3.f - ray.o), n) / n_dot_d;
  if (t < 0.f || t > ray.t)
    return false;
  
  // Point-in-triangle test
  vec3 p = ray.o + t * ray.d;
  return dot(n, cross(p - a, ab)) >= 0.f &&
         dot(n, cross(p - b, bc)) >= 0.f &&
         dot(n, cross(p - c, ca)) >= 0.f;
}

#endif // RAY_GLSL_GUARD