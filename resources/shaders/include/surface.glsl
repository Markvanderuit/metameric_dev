#ifndef GLSL_SURFACE_GUARD
#define GLSL_SURFACE_GUARD

#include <ray.glsl>
#include <gbuffer.glsl>
#include <frame.glsl>
#include <record.glsl>

// This header requires the following defines to point to SSBOs or shared memory
// to work around glsl's lack of ssbo argument passing
// #define srfc_buff_prim      buff_bvhs_prim.data
// #define srfc_buff_vert      buff_mesh_vert.data
// #define srfc_buff_elem      buff_mesh_elem.data
// #define srfc_buff_objc_info buff_objc_info.data
// #define srfc_buff_emtr_info buff_emtr_info.data
// #define srfc_buff_mesh_info buff_mesh_info.data

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

SurfaceInfo init_surface() {
  SurfaceInfo si;
  si.data = RECORD_INVALID_DATA;
  return si;
}

bool is_valid(in SurfaceInfo si) {
  return record_is_valid(si.data);
}

bool is_emitter(in SurfaceInfo si) {
  return record_is_emitter(si.data);
}

// Given a ray, and access to the scene's underlying primitive data,
// generate a SurfaceInfo object
SurfaceInfo get_surface_info(in Ray ray) {
  SurfaceInfo si;
  si.data = ray.data;
  
  // Early out if no hit data is present
  if (!record_is_valid(si.data))
    return si;

  if (record_is_object(si.data)) {
    // On a valid surface, fill in surface info
    ObjectInfo object_info = srfc_buff_objc_info[record_get_object(si.data)];
    MeshInfo   mesh_info   = srfc_buff_mesh_info[object_info.mesh_i];

    // Obtain and unpack intersected primitive data
    BVHPrim prim = unpack(srfc_buff_prim[mesh_info.prims_offs + record_get_object_primitive(ray.data)]);
      
    // Compute geometric normal
    si.n = cross(prim.v1.p - prim.v0.p, prim.v2.p - prim.v1.p);
    
    // Compute object-space surface position
    vec3 p = (object_info.trf_mesh_inv * vec4(ray.o + ray.d * ray.t, 1)).xyz;
    
    // Reinterpolate surface position, texture coordinates, shading normal using barycentrics
    vec3 b  = gen_barycentric_coords(p, prim.v0.p, prim.v1.p, prim.v2.p);
    vec3 ns = b.x * prim.v0.n  + b.y * prim.v1.n  + b.z * prim.v2.n;
    si.p    = b.x * prim.v0.p  + b.y * prim.v1.p  + b.z * prim.v2.p;
    si.tx   = b.x * prim.v0.tx + b.y * prim.v1.tx + b.z * prim.v2.tx;

    // Transform relevant data to world-space
    ns   = (object_info.trf_mesh * vec4(ns, 0)).xyz;
    si.p = (object_info.trf_mesh * vec4(si.p, 1)).xyz;
    si.n = (object_info.trf_mesh * vec4(si.n, 0)).xyz;

    // Normalize vectors as transformations don't preserve this :/
    si.n = normalize(si.n);
    ns   = normalize(ns);

    // Flip normals on back hit
    if (dot(si.n, ray.d) > 0) si.n = -si.n;
    if (dot(ns,   ray.d) > 0)   ns = -ns;

    // Generate shading frame
    si.sh = get_frame(normalize(ns));
    si.wi = frame_to_local(si.sh, -ray.d);
    si.t  = ray.t;
  } else if (record_is_emitter(si.data)) {
    // On a valid emitter, fill in surface info
    EmitterInfo em = srfc_buff_emtr_info[record_get_emitter(si.data)];
    
    // Fill positional data from ray hit
    si.p = ray_get_position(ray);

    // Fill normal data based on type of area emitters
    if (em.type == EmitterTypeSphere) {
      si.n = normalize(si.p - em.center);
    } else if (em.type == EmitterTypeRect) {
      si.n = em.rect_n;
    }
    
    // Flip normals on back hit
    // TODO maybe don't?
    // if (dot(si.n, ray.d) > 0) si.n = -si.n;

    // Generate shading frame
    si.sh = get_frame(si.n);
    si.wi = frame_to_local(si.sh, -ray.d);
    si.t  = ray.t;
  }

  return si;
}

// Given a ray, and access to the scene's underlying primitive data,
// generate a SurfaceInfo object
/* SurfaceInfo get_surface_info(in GBufferRay gb, in Ray ray) {
  SurfaceInfo si = init_surface();

  // Ensure the ray actually hit something
  if (gb.object_i == OBJECT_INVALID)
    return si;

  // On a valid surface, fill in surface info
  si.object_i = gb.object_i;
  
  ObjectInfo object_info = srfc_buff_objc_info[si.object_i];
  MeshInfo   mesh_info   = srfc_buff_mesh_info[object_info.mesh_i];

  // Obtain and unpack intersected primitive data
  uvec3 el = srfc_buff_elem[mesh_info.prims_offs + gb.primitive_i];
  BVHPrim prim = { unpack(srfc_buff_vert[el[0]]), 
                   unpack(srfc_buff_vert[el[1]]), 
                   unpack(srfc_buff_vert[el[2]]) };

  // Compute geometric normal
  si.n = cross(prim.v1.p - prim.v0.p, prim.v2.p - prim.v1.p);

  // Reinterpolate surface position, texture coordinates, shading normal using barycentrics
  vec3 b  = gen_barycentric_coords(gb.p, prim.v0.p, prim.v1.p, prim.v2.p);
  vec3 ns = b.x * prim.v0.n  + b.y * prim.v1.n  + b.z * prim.v2.n;
  si.p    = b.x * prim.v0.p  + b.y * prim.v1.p  + b.z * prim.v2.p;
  si.tx   = b.x * prim.v0.tx + b.y * prim.v1.tx + b.z * prim.v2.tx;

  // Transform relevant data to world-space
  ns   = (object_info.trf_mesh * vec4(ns, 0)).xyz;
  si.p = (object_info.trf_mesh * vec4(si.p, 1)).xyz;
  si.n = (object_info.trf_mesh * vec4(si.n, 0)).xyz;

  // Normalize vectors as transformations don't preserve this :/
  ns   = normalize(ns);
  si.n = normalize(si.n);
  
  // Flip normals on back hit
  if (dot(ns,   ray.d) > 0)   ns = -ns;
  if (dot(si.n, ray.d) > 0) si.n = -si.n;

  // Generate shading frame
  si.sh = get_frame(ns);
  si.wi = frame_to_local(si.sh, -ray.d);
  
  return si;
} */

vec3 surface_offset(in SurfaceInfo si, in vec3 d) {
  return fma(vec3(M_RAY_EPS), si.n, si.p);
}

PositionSample get_position_sample(in SurfaceInfo si) {
  PositionSample ps;

  ps.is_delta = false;
  ps.p        = si.p;
  ps.n        = si.n;
  ps.d        = frame_to_world(si.sh, -si.wi);
  ps.data     = si.data;
  ps.t        = si.t;

  return ps;
}

#endif // GLSL_SURFACE_GUARD