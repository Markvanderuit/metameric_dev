#ifndef SHAPE_RECTANGLE_GLSL_GUARD
#define SHAPE_RECTANGLE_GLSL_GUARD

#include <render/ray.glsl>

bool ray_intersect(inout Ray ray, in vec3 c, in vec3 n, in mat4 trf_inv) {
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

bool ray_intersect_unit_rectangle(inout Ray ray) {
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

#endif // SHAPE_RECTANGLE_GLSL_GUARD