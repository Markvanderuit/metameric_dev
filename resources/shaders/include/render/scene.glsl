#ifndef SCENE_GLSL_GUARD
#define SCENE_GLSL_GUARD

#include <render/detail/scene_types.glsl> // Should be available by now
#include <render/ray.glsl>
#include <render/bvh.glsl>
#include <render/tlas.glsl>
#include <render/emitter.glsl>
#include <render/object.glsl>
#include <render/brdf.glsl>

bool scene_intersect(inout Ray ray) {
  for (uint i = 0; i < scene_object_count(); ++i) {
    ray_intersect_object(ray, i);
  }
  for (uint i = 0; i < scene_emitter_count(); ++i) {
    ray_intersect_emitter(ray, i);
  }
  return is_valid(ray);
}

bool scene_intersect_any(in Ray ray) {
  for (uint i = 0; i < scene_object_count(); ++i) {
    if (ray_intersect_object_any(ray, i)) {
      return true;
    }
  }
  for (uint i = 0; i < scene_emitter_count(); ++i) {
    if (ray_intersect_emitter_any(ray, i)) {
      return true;
    }
  }
  return false;
}

#endif // SCENE_GLSL_GUARD