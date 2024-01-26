#ifndef SCENE_GLSL_GUARD
#define SCENE_GLSL_GUARD

#include <render/detail/scene_types.glsl> // Should be available by now
#include <render/bvh.glsl>
#include <render/emitter.glsl>
#include <render/object.glsl>
#include <render/brdf.glsl>

#ifdef SCENE_DATA_AVAILABLE

#include <render/ray.glsl>

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

#endif // SCENE_DATA_AVAILABLE
#endif // SCENE_GLSL_GUARD