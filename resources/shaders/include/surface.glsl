#ifndef GLSL_SURFACE_GUARD
#define GLSL_SURFACE_GUARD

#include <ray.glsl>

// This header requires the following defines to point to SSBOs or shared memory
// to work around glsl's lack of ssbo argument passing
// #define srfc_buff_prim buff_bvhs_prim.data

// An object defining a potential surface intersection in the scene.
// Is generally the output of ray_intersect(...).
struct SurfaceInfo {
  vec3  p;        // Surface position in world space
  vec3  n;        // Surface geometric normal
  vec3  ns;       // Surface shading normal
  vec2  tx;       // Surface texture coordinates
  uint  object_i; // Index of intersected object, set to OBJECT_INVALID if the intersection is invalid
};

// Given a triangle and a primitive, obtain barycentric coordiantes
vec3 gen_barycentric_coords(in vec3 p, in vec3 a, in vec3 b, in vec3 c) {
    vec3 ab = b - a;
    vec3 ac = c - a;

    float a_tri = abs(.5f * length(cross(ac, ab)));
    float a_ab  = abs(.5f * length(cross(p - a, ab)));
    float a_ac  = abs(.5f * length(cross(ac, p - a)));
    float a_bc  = abs(.5f * length(cross(c - p, b - p)));
    
    return vec3(a_bc, a_ac, a_ab) / a_tri;
}

// Given a ray, and access to the scene's underlying primitive data,
// generate a SurfaceInfo object
SurfaceInfo get_surface_info(in Ray ray) {
  SurfaceInfo si;
  si.object_i = get_ray_data_objc(ray);
  
  // On a valid surface, generate info
  if (si.object_i != OBJECT_INVALID) {
    // Obtain and unpack intersected primitive
    BVHPrim prim = unpack(srfc_buff_prim[get_ray_data_prim(ray)]);

    // Fill geometric data
    si.p = ray.o + ray.d * ray.t;
    si.n = normalize(cross(prim.v1.p - prim.v0.p, prim.v2.p - prim.v0.p));

    // Fill shading-dependent data
    vec3 b = gen_barycentric_coords(si.p, prim.v0.p, prim.v1.p, prim.v2.p);
    si.tx  = b.x * prim.v0.tx + b.y * prim.v1.tx + b.z * prim.v2.tx;
    si.ns  = b.x * prim.v0.n  + b.y * prim.v1.n  + b.z * prim.v2.n;
    si.ns  = normalize(si.n); // Why necessary? Grug find bug!
  }

  return si;
}

#endif // GLSL_SURFACE_GUARD