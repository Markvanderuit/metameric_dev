#ifndef RAY_GLSL_GUARD
#define RAY_GLSL_GUARD

#include <math.glsl>

struct AABB {
  vec3 minb; // Minimum of bounding box
  vec3 maxb; // Maximum of bounding box
};

// An object defining a 3-dimensional ray.
// Is generally the input of ray_intersect(...).
struct Ray {
  vec3  o;
  vec3  d;
  float t;
};

// Flag value to indicate invalid SurfaceInfo object
#define OBJECT_INVALID 0x0000FFFF // 65K specifically, as we pack the index in less precision

// An object defining a potential surface intersection in the scene.
// Is generally the output of ray_intersect(...).
struct SurfaceInfo {
  float t;        // Traversed ray distance to surface intersection
  vec3  p;        // Surface position in world space
  vec3  n;        // Surface geometric normal
  vec2  tx;       // Surface texture coordinates
  uint  object_i; // Index of intersected object, set to OBJECT_INVALID if the intersection is invalid
};

// Intermediate object to construct a SurfaceInfo if combined with a valid Ray.
// Is generally used internally in ray_intersect_*(...)
struct SurfaceInfoIntermediate {
  uint  object_i; // Index of intersected object, not mesh
  uint  prim_i;   // Index of intersected primitive
};

struct RayHit {
  Ray ray;       // Ray for which to run intersection; ray.t is updated
  uint object_i; // Index of intersected object, not mesh
  uint prim_i;   // Index of intersected primitive
};

#endif // RAY_GLSL_GUARD