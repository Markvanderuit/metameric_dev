#ifndef GLSL_SURFACE_GUARD
#define GLSL_SURFACE_GUARD

#include <render/ray.glsl>
#include <render/frame.glsl>
#include <render/record.glsl>

// An object defining a potential surface intersection in the scene.
// Is generally the output of ray_intersect(...).
struct SurfaceInfo {
  // Surface geometric information
  vec3 p;  // Surface position in world space
  vec3 n;  // Surface geometric normal
  vec2 tx; // Surface texture coordinates

  // Local shading information
  Frame sh; // Surface local shading frame, including shading normal
  vec3 wi;  // Incident direction in local shading frame
  float t;  // Distance traveled along ray in incident direction

  // Intersected object record; object/emitter index, primitive index
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
  
  // If hit data is present, forward to underlying surface type: object or emitter
  if (is_valid(si)) {
    if (is_object(si)) {
      detail_fill_surface_info_object(si, ray);
    } else if (is_emitter(si)) {
      detail_fill_surface_info_emitter(si, ray);
    } /* else {
      // ...
    } */
  }

  return si;
}

vec3 surface_offset(in SurfaceInfo si, in vec3 d) {
  return fma(vec3(M_RAY_EPS), si.n, si.p);
}

// Shorthands for frame transofrmation
vec3 to_local(in SurfaceInfo si, in vec3 v) { return to_local(si.sh, v); }
vec3 to_world(in SurfaceInfo si, in vec3 v) { return to_world(si.sh, v); }

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

PositionSample get_position_sample(in SurfaceInfo si) {
  PositionSample ps;

  ps.is_delta = false;
  ps.p        = si.p;
  ps.n        = si.n;
  ps.d        = -to_world(si, si.wi);
  ps.data     = si.data;
  ps.t        = si.t;

  return ps;
}

#endif // GLSL_SURFACE_GUARD