#ifndef SHAPE_SPHERE_GLSL_GUARD
#define SHAPE_SPHERE_GLSL_GUARD

#include <render/ray.glsl>

struct Sphere {
  vec3  center;
  float r;
};

bool ray_intersect(inout Ray ray, in Sphere sphere) {
  vec3  o = ray.o - sphere.center;
  float b = 2.f * dot(o, ray.d);
  float c = sdot(o) - sdot(sphere.r);
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

// Not exactly more efficient than ray_interspect_sphere
// should spend some time on this when I have it
bool ray_intersect_any(in Ray ray, in Sphere sphere) {
  vec3  o = ray.o - sphere.center;
  float b = 2.f * dot(o, ray.d);
  float c = sdot(o) - sdot(sphere.r);
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

#endif // SHAPE_SPHERE_GLSL_GUARD