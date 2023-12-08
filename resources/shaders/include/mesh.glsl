#ifndef GLSL_MESH_GUARD
#define GLSL_MESH_GUARD

#include <math.glsl>

// Basic AOS vertex data
struct MeshVertex {
  vec3 p;  // World-space position
  vec3 n;  // World-space surface normal
  vec2 tx; // Surface texture coordinate
};

// Generate mesh vertex data from packed inputs
MeshVertex decode_mesh_vertex(in vec4 pack_a, in vec4 pack_b) {
  MeshVertex v;
  v.p  = pack_a.xyz;
  v.n  = pack_b.xyz;
  v.tx = vec2(pack_a.w, pack_b.w);
  return v;
}

#endif // GLSL_MESH_GUARD
