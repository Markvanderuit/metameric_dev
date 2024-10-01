#ifndef RAY_GLSL_GUARD
#define RAY_GLSL_GUARD

#include <math.glsl>
#include <render/record.glsl>
#include <sampler/uniform.glsl>

// An object defining a 3-dimensional ray.
// Is generally the input of ray_intersect(...)
// With minor output data packed in padded space
struct Ray {
  vec3  o;
  float t;
  vec3  d;

  // Intersected object record, or anyhit information; see record.glsl
  uint data;
};

Ray init_ray(vec3 d) {
  Ray ray;
  ray.o    = vec3(0);
  ray.d    = d;
  ray.t    = FLT_MAX;
  record_clear(ray.data);
  return ray;
}

Ray init_ray(vec3 d, float t_max) {
  Ray ray;
  ray.o    = vec3(0);
  ray.d    = d;
  ray.t    = t_max;
  record_clear(ray.data);
  return ray;
}

Ray init_ray(vec3 o, vec3 d) {
  Ray ray;
  ray.o    = o;
  ray.d    = d;
  ray.t    = FLT_MAX;
  record_clear(ray.data);
  return ray;
}

Ray init_ray(vec3 o, vec3 d, float t_max) {
  Ray ray;
  ray.o    = o;
  ray.d    = d;
  ray.t    = t_max;
  record_clear(ray.data);
  return ray;
}

bool is_valid(in Ray ray) {
  return ray.t != FLT_MAX && record_is_valid(ray.data);
}

bool is_anyhit(in Ray ray) {
  return record_get_anyhit(ray.data);
}

bool hit_object(in Ray ray) {
  return record_is_object(ray.data);
}

bool hit_emitter(in Ray ray) {
  return record_is_emitter(ray.data);
}

vec3 ray_get_position(in Ray ray) {
  return ray.t == FLT_MAX ? vec3(FLT_MAX) : ray.o + ray.d * ray.t;
}

Ray ray_to_local(in Ray ray, in mat4 to_local) {  
  // Generate local ray
  Ray ray_local;
  ray_local.o = (to_local * vec4(ray.o, 1)).xyz;
  ray_local.d = (to_local * vec4(ray.d, 0)).xyz;

  // Get length and normalize direction
  // Reuse length to adjust ray_local.t if ray.t is not at infty
  float dt = length(ray_local.d);
  ray_local.d /= dt;
  ray_local.t = (ray.t == FLT_MAX) ? FLT_MAX : dt * ray.t;

  return ray_local;
}

void ray_to_world_inplace(inout Ray ray, in Ray ray_local, in mat4 to_world) {
  ray.t    = length((to_world * vec4(ray_local.d * ray_local.t, 0)).xyz);
  ray.data = ray_local.data;
}

Ray ray_to_world(in Ray ray_local, in mat4 to_world) {
  Ray ray;
  ray.o = (to_world * vec4(ray_local.o, 1)).xyz;
  ray.d = (to_world * vec4(ray_local.d, 0)).xyz;
  ray_to_world_inplace(ray, ray_local, to_world);
  return ray;
}

#endif // RAY_GLSL_GUARD