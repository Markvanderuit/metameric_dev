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

struct PathInfoSOAPack0 { vec4 data; };
struct PathInfoSOAPack1 { vec4 data; };
struct PathInfoSOAPack2 { vec4 data; };

// The PathInfo object stores the general state for a path that is in-flight.
// If a path is terminated, values are reset, but pixel/state are preserved.
// Size: 
struct PathInfo {
  uvec2 px;         // Index of pixel to add path into
  uint  state;      // Sampler state
  
  float result;     // Accumulated result,          initialized to 0 for new paths
  float wavelength; // Sampled wavelength,          regenerated for new paths
  
  uint  depth;      // Current length of path,      initialized to 0 for new paths
  bool  scattered;  // Did the path scatter,        initialized to 0        
  float p;          // Current probability of path, initialized to 1 for new paths
  float throughput; // Current throughput of path,  initialized to 1 for new paths
  float eta;        // Current refractive index,    initialized to 1 for new paths
};

// Info object for querying an extension ray cast.
// Size: 32 bytes
struct PathExtendInfo {
  vec3  o;          // Ray origin
  uint  i;          // Index of path for which raycast is performed
  vec3  d;          // Ray direction;
  float t_max;      // Ray extent, initialized to FLT_MAX 
};

// Info object for querying a shadow ray cast.
// Size: 32 bytes
struct PathShadowInfo {
  vec3  o;          // Ray origin
  uint  i;          // Index of path for which raycast is performed
  vec3  d;          // Ray direction;
  float t_max;      // Ray extent, initialized to FLT_MAX 
};

// Info object for querying a shading operation.
// Size: 32 bytes
struct PathShadingInfo {
  uint object_i;    // Index of object on which to evaluate shading
  uint mesh_i;      // Index of mesh primitive on which to evaluate shading
  uint i;           // Index of path for which to evaluate shading
  uint padding0;
  vec3 p;           // World-space position at which to evaluate shading
  uint padding1;
};
#endif // RAY_GLSL_GUARD