#ifndef GLSL_SURFACE_GUARD
#define GLSL_SURFACE_GUARD

#include <render/ray.glsl>
#include <render/frame.glsl>
#include <render/record.glsl>

// An object defining a generic ray intersection in the scene.
// Also defines no intersection (e.g. envmaps in the distance)
// Generally the output of ray_intersect(...)
struct IntersectionInfo {
  vec3 p;  // Potential intersected position, can be undefined
  vec3 n;  // Normal defining intersection-local frame
  vec2 tx; // Surface/emitter texture coordinates
  vec3 wi; // Incident direction in local frame
  float t; // Distance traveled along ray in incident direction

  // Intersected object/emitter record; validity, type, entity index, optionally underlying primitive
  uint data;
};

// An object defining a potential surface intersection in the scene.
// Is generally the output of ray_intersect(...).
struct SurfaceInfo {
  // Surface information
  vec3 p;  // Surface position in world space
  vec2 tx; // Surface texture coordinates
  vec3 n;  // Surface shading normal, defines local frame
 
  // Ray information
  vec3 wi; // Incident direction in local frame
  float t; // Distance traveled along incident direction
  
  // Intersection record; object/emitter index, primitive index
  uint data;
};

// Surface type queries
bool is_valid(in SurfaceInfo si)   { return record_is_valid(si.data); }
bool is_object(in SurfaceInfo si)  { return record_is_object(si.data); }
bool is_emitter(in SurfaceInfo si) { return record_is_emitter(si.data); }

#include <render/detail/surface.glsl>

// Given a ray, and access to the scene's underlying primitive data,
// generate a SurfaceInfo object
SurfaceInfo get_surface_info(in Ray ray) {
  SurfaceInfo si;
  si.data = ray.data;
  
  if (is_valid(si)) {
    // If hit data is present, forward to underlying surface type: object or emitter
    if (is_object(si)) {
      detail_fill_surface_info_object(si, ray);
    } else if (is_emitter(si)) {
      detail_fill_surface_info_emitter(si, ray);
    }
  } else if (scene_has_envm_emitter()) {
    // Otherwise, fill info for fallback interaction type: envmap
    detail_fill_surface_info_envmap(si, ray);
  }

  return si;
}

// Offset above/below surface depending on the incidence of the ray direction
vec3 surface_offset(in SurfaceInfo si, in vec3 d) {
  if (dot(si.n, d) >= 0)
    return si.p + si.n * M_RAY_EPS;
  else
    return si.p - si.n * M_RAY_EPS;
}

// Shorthands for frame transformation
vec3 to_local(in SurfaceInfo si, in vec3 v) { return to_local(get_frame(si.n), v); }
vec3 to_world(in SurfaceInfo si, in vec3 v) { return to_world(get_frame(si.n), v); }

Ray ray_towards_direction(in SurfaceInfo si, in vec3 d) {
  return init_ray(surface_offset(si, d), d);
}

Ray ray_towards_point(in SurfaceInfo si, in vec3 p) {
  Ray ray;
  
  ray.o = surface_offset(si, p - si.p);
  ray.d = p - ray.o;
  ray.t = length(ray.d);
  ray.d /= ray.t;
  ray.t *= (1.f - M_RAY_EPS * 10.f);
  ray.data = RECORD_INVALID_DATA;

  return ray;
}

#endif // GLSL_SURFACE_GUARD