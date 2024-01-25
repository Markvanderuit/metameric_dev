#ifndef SHAPE_PRIMITIVE_GLSL_GUARD
#define SHAPE_PRIMITIVE_GLSL_GUARD

#include <render/detail/packing.glsl>
#include <render/ray.glsl>

struct Primitive {
  vec3 a, b, c;
};

Primitive unpack_primitive(in MeshPrimPack pack) {
  Primitive prim;
  prim.a = vec3(unpackUnorm2x16(pack.v0.p0), unpackSnorm2x16(pack.v0.p1).x);
  prim.b = vec3(unpackUnorm2x16(pack.v1.p0), unpackSnorm2x16(pack.v1.p1).x);
  prim.c = vec3(unpackUnorm2x16(pack.v2.p0), unpackSnorm2x16(pack.v2.p1).x);
  return prim;
}

bool ray_intersect(inout Ray ray, in Primitive prim) {
  vec3 ab = prim.b - prim.a;
  vec3 bc = prim.c - prim.b;
  vec3 ca = prim.a - prim.c;
  vec3 n  = normalize(cross(bc, ab));
  
  // Backface test
  float cos_theta = dot(n, ray.d);
  /* if (cos_theta <= 0)
    return false; */

  // Ray/plane distance test
  float t = dot(((prim.a + prim.b + prim.c) / 3.f - ray.o), n) / cos_theta;
  if (t < 0.f || t > ray.t)
    return false;
  
  // Point-in-triangle test
  vec3 p = ray.o + t * ray.d;
  if ((dot(n, cross(p - prim.a, ab)) < 0.f) ||
      (dot(n, cross(p - prim.b, bc)) < 0.f) ||
      (dot(n, cross(p - prim.c, ca)) < 0.f))
    return false;

  // Update closest-hit distance before return
  ray.t = t;
  return true;
}

#endif // SHAPE_PRIMITIVE_GLSL_GUARD