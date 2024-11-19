#ifndef SCENE_GLSL_GUARD
#define SCENE_GLSL_GUARD

#include <render/tlas.glsl>

bool scene_intersect(inout Ray ray) {
#ifdef SCENE_DATA_TLAS
  return ray_intersect_tlas(ray);
#else // SCENE_DATA_TLAS
  // Alternatively; loop all objects
  for (uint i = 0; i < scene_object_count(); ++i) {
    ray_intersect_object(ray, i);
  }
  for (uint i = 0; i < scene_emitter_count(); ++i) {
    ray_intersect_emitter(ray, i);
  }
  return is_valid(ray);
#endif // SCENE_DATA_TLAS
}

bool scene_intersect_any(in Ray ray) {
#ifdef SCENE_DATA_TLAS
  return ray_intersect_tlas_any(ray);
#else // SCENE_DATA_TLAS
  // Alternatively; loop all objects
  for (uint i = 0; i < scene_object_count(); ++i) {
    if (ray_intersect_object_any(ray, i))
      return true;
  }
  for (uint i = 0; i < scene_emitter_count(); ++i) {
    if (ray_intersect_emitter_any(ray, i))
      return true;
  }
  return false;
#endif // SCENE_DATA_TLAS
}

#endif // SCENE_GLSL_GUARD