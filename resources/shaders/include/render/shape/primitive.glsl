#ifndef SHAPE_PRIMITIVE_GLSL_GUARD
#define SHAPE_PRIMITIVE_GLSL_GUARD

#include <render/ray.glsl>

// Scene vertex; position, normal, and texture coordinate
struct Vertex {
  vec3 p;
  vec3 n;
  vec2 tx;
};

// Scene primitive; a triangle of vertices
struct Primitive {
  Vertex v0, v1, v2;
};

// Scene primitive; vertex positions only
struct Triangle {
  vec3 p0, p1, p2;
};

// Unpack Vertex data from a packed representation
Vertex unpack(in VertexPack p) {
  Vertex o;
  o.p  = vec3(unpackUnorm2x16(p.p0),   unpackSnorm2x16(p.p1).x);
  o.n  = normalize(vec3(unpackSnorm2x16(p.p1).y, unpackSnorm2x16(p.n)));
  o.tx = unpackUnorm2x16(p.tx);
  return o;
}

// Unpack Primitive data from a packed representation
Primitive unpack(in PrimitivePack p) {
  Primitive prim = { 
    unpack(p.v0),
    unpack(p.v1),
    unpack(p.v2)
  };
  return prim;
}

// Unpack only Primitive position data from a packed representation
Triangle unpack_triangle(in PrimitivePack pack) {
  Triangle prim = {
    vec3(unpackUnorm2x16(pack.v0.p0), unpackSnorm2x16(pack.v0.p1).x),
    vec3(unpackUnorm2x16(pack.v1.p0), unpackSnorm2x16(pack.v1.p1).x),
    vec3(unpackUnorm2x16(pack.v2.p0), unpackSnorm2x16(pack.v2.p1).x)
  };
  return prim;
}

bool ray_intersect(inout Ray ray, in Primitive prim) {
  vec3 ab = prim.v1.p - prim.v0.p;
  vec3 bc = prim.v2.p - prim.v1.p;
  vec3 n_ = cross(bc, ab);
  
  // Ray/plane distance test
  float t = dot(((prim.v0.p + prim.v1.p + prim.v2.p) / 3.f - ray.o), n_) 
          / dot(n_, ray.d);
  if (t < 0.f || t > ray.t)
    return false;
  
  // Point-in-triangle test
  vec3 p = fma(ray.d, vec3(t), ray.o);
  if ((dot(n_, cross(p - prim.v0.p,                    ab)) < 0.f) ||
      (dot(n_, cross(p - prim.v1.p,                    bc)) < 0.f) ||
      (dot(n_, cross(p - prim.v2.p, prim.v0.p - prim.v2.p)) < 0.f))
    return false;

  // Update closest-hit distance before return
  ray.t = t;
  return true;
}

bool ray_intersect(inout Ray ray, in Triangle prim) {
  vec3 ab = prim.p1 - prim.p0;
  vec3 bc = prim.p2 - prim.p1;
  vec3 n_ = cross(bc, ab);
  
  // Ray/plane distance test
  float t = dot(((prim.p0 + prim.p1 + prim.p2) / 3.f - ray.o), n_) 
          / dot(n_, ray.d);
  if (t < 0.f || t > ray.t)
    return false;
  
  // Point-in-triangle test
  vec3 p = fma(ray.d, vec3(t), ray.o);
  if ((dot(n_, cross(p - prim.p0,                ab)) < 0.f) ||
      (dot(n_, cross(p - prim.p1,                bc)) < 0.f) ||
      (dot(n_, cross(p - prim.p2, prim.p0 - prim.p2)) < 0.f))
    return false;

  // Update closest-hit distance before return
  ray.t = t;
  return true;
}

#endif // SHAPE_PRIMITIVE_GLSL_GUARD