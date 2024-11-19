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
  ray.o = vec3(0);
  ray.d = d;
  ray.t = t_max;
  record_clear(ray.data);
  return ray;
}

Ray init_ray(vec3 o, vec3 d) {
  Ray ray;
  ray.o = o;
  ray.d = d;
  ray.t = FLT_MAX;
  record_clear(ray.data);
  return ray;
}

Ray init_ray(vec3 o, vec3 d, float t_max) {
  Ray ray;
  ray.o = o;
  ray.d = d;
  ray.t = t_max;
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

Ray ray_transform(in Ray ray_world, in mat4 to_local) {  
  // Generate local ray
  Ray ray_local;
  ray_local.o = (to_local * vec4(ray_world.o, 1)).xyz;
  ray_local.d = (to_local * vec4(ray_world.d, 0)).xyz;

  // Get length and normalize direction
  // Reuse length to adjust ray_local.t if ray.t is not at infty
  float dt = length(ray_local.d);
  ray_local.d /= dt;
  ray_local.t = ray_world.t == FLT_MAX 
              ? FLT_MAX 
              : dt * ray_world.t;
  ray_local.data = ray_world.data;
  
  return ray_local;
}

void ray_transform_inplace(inout Ray ray_world, in Ray ray_local, in mat4 to_world) {
  ray_world.t    = ray_local.t == FLT_MAX
                 ? FLT_MAX
                 : length((to_world * vec4(ray_local.d * ray_local.t, 0)).xyz);
  ray_world.data = ray_local.data;
}

#endif // RAY_GLSL_GUARD