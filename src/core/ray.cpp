#include <metameric/core/mesh.hpp>
#include <metameric/core/ray.hpp>
#include <limits>

namespace met {
  template <typename Mesh>
  RayQuery raytrace_vert(const Ray &ray, const Mesh &mesh, float min_distance) {
    met_trace();

    const auto &[verts, elems, _norms, _uvs] = mesh;
    
    RayQuery query;
    float min_distance_2 = min_distance * min_distance;

    for (uint i = 0; i < verts.size(); ++i) {
      eig::Vector3f v = verts[i];
      float         t = (v - ray.o).dot(ray.d);
      guard_continue(t >= 0.f && t < query.t);

      eig::Vector3f x = ray.o + t * ray.d;
      guard_continue((v - x).squaredNorm() <= min_distance_2);

      query = { t, i };
    }

    return query;
  }
  
  template <typename Mesh>
  RayQuery raytrace_elem(const Ray &ray, const Mesh &mesh, bool cull_backface) {
    met_trace();

    const auto &[verts, elems, _norms, _uvs] = mesh;

    RayQuery query;

    for (uint i = 0; i < elems.size(); ++i) {
      // Load triangle data
      const eig::Array3u e = elems[i];
      eig::Vector3f a = verts[e[0]], 
                    b = verts[e[1]], 
                    c = verts[e[2]];

      // Compute edges, plane normal
      eig::Vector3f ab = b - a, bc = c - b, ca = a - c;
      eig::Vector3f n  = bc.cross(ab).normalized();
      
      // Test if intersected plane is front-facing
      float n_dot_d = n.dot(ray.d);
      guard_continue(!cull_backface || n_dot_d > 0.f);

      // Test if intersection point is closer than current t
      float t = ((a + b + c) / 3.f - ray.o).dot(n) / n_dot_d;
      guard_continue(t >= 0.f && t < query.t);

      // Test if intersection point lies within triangle boundaries
      eig::Vector3f x = ray.o + t * ray.d;
      guard_continue(n.dot((x - a).cross(ab)) >= 0.f);
      guard_continue(n.dot((x - b).cross(bc)) >= 0.f);
      guard_continue(n.dot((x - c).cross(ca)) >= 0.f);

      query = { t, i };
    }

    return query;
  }

  /* Explicit template instantiations */

  template
  RayQuery raytrace_vert<Mesh>(const Ray &, const Mesh &, float);
  template
  RayQuery raytrace_vert<AlMesh>(const Ray &, const AlMesh &, float);
  template
  RayQuery raytrace_elem<Mesh>(const Ray &, const Mesh &, bool);
  template
  RayQuery raytrace_elem<AlMesh>(const Ray &, const AlMesh &, bool);
} // namespace met