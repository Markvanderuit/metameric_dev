#ifndef SCENE_GLSL_GUARD
#define SCENE_GLSL_GUARD

#include <render/tlas.glsl>

bool scene_intersect(inout Ray ray) {
#ifdef LOAD_TLAS_GLSL_GUARD
  return ray_intersect_tlas(ray);
#else // LOAD_TLAS_GLSL_GUARD
  // Alternatively; loop all objects
  for (uint i = 0; i < scene_object_count(); ++i) {
    ray_intersect_object(ray, i);
  }
  for (uint i = 0; i < scene_emitter_count(); ++i) {
    ray_intersect_emitter(ray, i);
  }
  return is_valid(ray);
#endif // LOAD_TLAS_GLSL_GUARD
}

bool scene_intersect_any(in Ray ray) {
#ifdef LOAD_TLAS_GLSL_GUARD
  return ray_intersect_tlas_any(ray);
#else // LOAD_TLAS_GLSL_GUARD
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
#endif // LOAD_TLAS_GLSL_GUARD
}

#endif // SCENE_GLSL_GUARD