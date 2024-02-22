#ifndef SHAPE_PRIMITIVE_GLSL_GUARD
#define SHAPE_PRIMITIVE_GLSL_GUARD

#include <render/ray.glsl>

// Unpacked vertex data
struct Vertex {
  vec3 p;
  vec3 n;
  vec2 tx;
};

// Unpacked primitive data, typically queried during bvh travesal
struct Primitive {
  Vertex v0;
  Vertex v1;
  Vertex v2;
};

// Unpacked primitive data, positions only
struct PrimitivePositions {
  vec3 p0, p1, p2;
};

MeshVertPack to_mesh_vert_pack(in uvec4 v) {
  MeshVertPack p;
  p.p0 = v.x;
  p.p1 = v.y;
  p.n  = v.z;
  p.tx = v.w;
  return p;
}

Vertex unpack(in MeshVertPack p) {
  Vertex o;
  o.p  = vec3(unpackUnorm2x16(p.p0),   unpackSnorm2x16(p.p1).x);
  o.n  = normalize(vec3(unpackSnorm2x16(p.p1).y, unpackSnorm2x16(p.n)));
  o.tx = unpackUnorm2x16(p.tx);
  return o;
}

Primitive unpack(in MeshPrimPack p) {
  Primitive o;
  o.v0 = unpack(p.v0);
  o.v1 = unpack(p.v1);
  o.v2 = unpack(p.v2);
  return o;
}

PrimitivePositions unpack_positions(in MeshPrimPack pack) {
  PrimitivePositions prim;
  prim.p0 = vec3(unpackUnorm2x16(pack.v0.p0), unpackSnorm2x16(pack.v0.p1).x);
  prim.p1 = vec3(unpackUnorm2x16(pack.v1.p0), unpackSnorm2x16(pack.v1.p1).x);
  prim.p2 = vec3(unpackUnorm2x16(pack.v2.p0), unpackSnorm2x16(pack.v2.p1).x);
  return prim;
}

bool ray_intersect(inout Ray ray, in Primitive prim) {
  vec3 ab = prim.v1.p - prim.v0.p;
  vec3 bc = prim.v2.p - prim.v1.p;
  vec3 ca = prim.v0.p - prim.v2.p;
  vec3 n  = normalize(cross(bc, ab));
  
  // Backface test
  float cos_theta = dot(n, ray.d);
  /* if (cos_theta <= 0)
    return false; */

  // Ray/plane distance test
  float t = dot(((prim.v0.p + prim.v1.p + prim.v2.p) / 3.f - ray.o), n) / cos_theta;
  if (t < 0.f || t > ray.t)
    return false;
  
  // Point-in-triangle test
  vec3 p = ray.o + t * ray.d;
  if ((dot(n, cross(p - prim.v0.p, ab)) < 0.f) ||
      (dot(n, cross(p - prim.v1.p, bc)) < 0.f) ||
      (dot(n, cross(p - prim.v2.p, ca)) < 0.f))
    return false;

  // Update closest-hit distance before return
  ray.t = t;
  return true;
}

bool ray_intersect(inout Ray ray, in PrimitivePositions prim) {
  vec3 ab = prim.p1 - prim.p0;
  vec3 bc = prim.p2 - prim.p1;
  vec3 ca = prim.p0 - prim.p2;
  vec3 n  = normalize(cross(bc, ab));
  
  // Backface test
  float cos_theta = dot(n, ray.d);
  /* if (cos_theta <= 0)
    return false; */

  // Ray/plane distance test
  float t = dot(((prim.p0 + prim.p1 + prim.p2) / 3.f - ray.o), n) / cos_theta;
  if (t < 0.f || t > ray.t)
    return false;
  
  // Point-in-triangle test
  vec3 p = ray.o + t * ray.d;
  if ((dot(n, cross(p - prim.p0, ab)) < 0.f) ||
      (dot(n, cross(p - prim.p1, bc)) < 0.f) ||
      (dot(n, cross(p - prim.p2, ca)) < 0.f))
    return false;

  // Update closest-hit distance before return
  ray.t = t;
  return true;
}

#endif // SHAPE_PRIMITIVE_GLSL_GUARD