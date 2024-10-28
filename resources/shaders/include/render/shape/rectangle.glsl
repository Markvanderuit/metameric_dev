#ifndef SHAPE_RECTANGLE_GLSL_GUARD
#define SHAPE_RECTANGLE_GLSL_GUARD

#include <render/ray.glsl>

struct Rectangle {
  vec3 p; // corner position
  vec3 u; // first edge
  vec3 v; // edge
  vec3 n; // normal
};

bool ray_intersect(inout Ray ray, in Rectangle rect) {
  // Plane distance test
  float t = (dot(rect.p, rect.n) - dot(ray.o, rect.n)) / dot(ray.d, rect.n);
  if (t < 0.f || t > ray.t)
    return false;

  // Find plane intersection
  vec3 p = ray. o + ray.d * t;

  // Plane boundary test
  vec2 wh   = vec2(length(rect.u), length(rect.v));
  vec3 q    = p - rect.p;
  vec2 proj = vec2(dot(rect.u / wh.x, q), dot(rect.v / wh.y, q));
  if (clamp(proj, -.5 * wh, .5 * wh) != proj)
    return false;
  
  ray.t = t;
  return true;
}

bool ray_intersect_any(in Ray ray, in Rectangle rect) {
  // Plane distance test
  float t = (dot(rect.p, rect.n) - dot(ray.o, rect.n)) / dot(ray.d, rect.n);
  if (t < 0.f || t > ray.t)
    return false;

  // Find plane intersection
  vec3 p = ray. o + ray.d * t;

  // Plane boundary test
  vec2 wh   = vec2(length(rect.u), length(rect.v));
  vec3 q    = p - rect.p;
  vec2 proj = vec2(dot(rect.u / wh.x, q), dot(rect.v / wh.y, q));
  return clamp(proj, -.5 * wh, .5 * wh) == proj;
}

#endif // SHAPE_RECTANGLE_GLSL_GUARD