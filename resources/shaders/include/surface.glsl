#ifndef GLSL_SURFACE_GUARD
#define GLSL_SURFACE_GUARD

#include <ray.glsl>
#include <gbuffer.glsl>

// This header requires the following defines to point to SSBOs or shared memory
// to work around glsl's lack of ssbo argument passing
// #define srfc_buff_prim      buff_bvhs_prim.data
// #define srfc_buff_objc_info buff_objc_info.data
// #define srfc_buff_mesh_info buff_mesh_info.data

// Local vector frame
struct Frame {
  vec3 n, s, t;
};

Frame create_frame(in vec3 n) {
  Frame fr;
  fr.n = n;

  float s = sign(n.z);
  float a = -1.f / (s + n.z);
  float b = n.x * n.y * a; 

  fr.s = vec3((n.x * n.x * a)  *  sign(n.z) + 1,
              b                *  sign(n.z),
              n.x              * -sign(n.z));
  fr.t = vec3(b,
              fma(n.y, n.y * a, s),
             -n.y);
  return fr;
}

vec3 to_local(in Frame fr, in vec3 v) {
  return vec3(dot(v, fr.s), dot(v, fr.t), dot(v, fr.n));
}

vec3 to_world(in Frame fr, in vec3 v) {
  return fma(fr.n, vec3(v.z), fma(fr.t, vec3(v.y), fr.s * v.x));
}

float local_cos_theta(in vec3 v) {
  return v.z;
}

// An object defining a potential surface intersection in the scene.
// Is generally the output of ray_intersect(...).
struct SurfaceInfo {
  vec3 p;        // Surface position in world space
  vec3 n;        // Surface geometric normal
  vec3 ns;       // Surface shading normal
  vec2 tx;       // Surface texture coordinates
  uint object_i; // Index of intersected object, set to OBJECT_INVALID if the intersection is invalid
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
  
  // On a valid surface, fill in surface info
  if (si.object_i != OBJECT_INVALID) {
    ObjectInfo object_info = srfc_buff_objc_info[si.object_i];
    MeshInfo   mesh_info   = srfc_buff_mesh_info[object_info.mesh_i];

    // Compute model-space surface position
    vec3 p = (object_info.trf_inv * vec4(ray.o + ray.d * ray.t, 1)).xyz;

    // Obtain and unpack intersected primitive data
    BVHPrim prim = unpack(srfc_buff_prim[mesh_info.prims_offs + get_ray_data_prim(ray)]);

    // Compute geometric normal
    si.n = normalize(cross(prim.v1.p - prim.v0.p, prim.v2.p - prim.v1.p));
    
    // Reinterpolate surface position, texture coordinates, shading normal using barycentrics
    vec3 b = gen_barycentric_coords(p, prim.v0.p, prim.v1.p, prim.v2.p);
    si.p  = b.x * prim.v0.p  + b.y * prim.v1.p  + b.z * prim.v2.p;
    si.tx = b.x * prim.v0.tx + b.y * prim.v1.tx + b.z * prim.v2.tx;
    si.ns = b.x * prim.v0.n  + b.y * prim.v1.n  + b.z * prim.v2.n;

    // Transform to world-space
    si.p  = (object_info.trf * vec4(si.p, 1)).xyz;
    si.n  = normalize((object_info.trf * vec4(si.n,  0)).xyz);
    si.ns = normalize((object_info.trf * vec4(si.ns, 0)).xyz);
  }

  return si;
}

#endif // GLSL_SURFACE_GUARD