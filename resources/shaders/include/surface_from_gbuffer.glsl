#ifndef GLSL_SURFACE_FROM_GBUFFER_GUARD
#define GLSL_SURFACE_FROM_GBUFFER_GUARD

#include <gbuffer.glsl>
#include <surface.glsl>

// This header requires the following defines to point to SSBOs or shared memory
// to work around glsl's lack of ssbo argument passing
// #define srfc_buff_objc_info buff_objc_info.data
// #define srfc_buff_mesh_info buff_mesh_info.data
// #define srfc_buff_vert      buff_mesh_vert.data
// #define srfc_buff_elem      buff_mesh_elem.data

// Given a ray, and access to the scene's underlying primitive data,
// generate a SurfaceInfo object
SurfaceInfo get_surface_info(in GBufferRay gb) {
  SurfaceInfo si;
  si.object_i = gb.object_i;
  
  // On a valid surface, fill in surface info
  if (si.object_i != OBJECT_INVALID) {
    ObjectInfo object_info = srfc_buff_objc_info[si.object_i];
    MeshInfo   mesh_info   = srfc_buff_mesh_info[object_info.mesh_i];

    // Obtain and unpack intersected primitive data
    uvec3 el = srfc_buff_elem[mesh_info.prims_offs + gb.primitive_i];
    BVHPrim prim = { unpack(srfc_buff_vert[el[0]]), 
                     unpack(srfc_buff_vert[el[1]]), 
                     unpack(srfc_buff_vert[el[2]]) };

    // Compute geometric normal
    si.n = normalize(cross(prim.v1.p - prim.v0.p, prim.v2.p - prim.v1.p));

    // Reinterpolate surface position, texture coordinates, shading normal using barycentrics
    vec3 b = gen_barycentric_coords(gb.p, prim.v0.p, prim.v1.p, prim.v2.p);
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

#endif // GLSL_SURFACE_FROM_GBUFFER_GUARD