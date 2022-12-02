#pragma once

#include <limits>
#include <metameric/core/math.hpp>
#include <metameric/core/mesh.hpp>

namespace met {
  constexpr float ray_maximum = std::numeric_limits<float>::max();
  constexpr float ray_epsilon = .00001f;

  // Simple ray tracing structure: origin, direction vectors
  struct Ray { eig::Vector3f o, d; };

  // Simple ray query object, returned on ray trace operation
  struct RayQuery {
    // Distance to intersected position
    float t = ray_maximum;

    // Index of relevant query object
    uint i;

    // Validity querying
    bool is_valid() const { return t != ray_maximum; }
    explicit operator bool() const { return t != ray_maximum; } 
  };

  // Given a ray object, find the nearest vertex alongside the ray within min_distance
  inline
  RayQuery ray_trace_nearest_vert(const Ray &ray,
                                  const std::vector<eig::Array3f> &verts,
                                  float min_distance = 0.025f) {
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

  // Given a ray object, find the nearest vertex alongside the ray within min_distance
  template <typename Traits>
  inline
  RayQuery ray_trace_nearest_vert(const Ray &ray,
                                  const TriMesh<Traits> &mesh,
                                  float min_distance = 0.025f) {
    RayQuery query;
    float min_distance_2 = min_distance * min_distance;

    for (auto vh : mesh.vertices()) {
      eig::Vector3f v = to_eig<float, 3>(mesh.point(vh));
      float         t = (v - ray.o).dot(ray.d);
      guard_continue(t >= 0.f && t < query.t);

      eig::Vector3f x = ray.o + t * ray.d;
      guard_continue((v - x).squaredNorm() <= min_distance_2);

      query = { t, static_cast<uint>(vh.idx()) };
    }

    return query;
  }

  // Given a ray object, find the nearest front-facing triangle intersecting the ray
  inline
  RayQuery ray_trace_nearest_elem(const Ray &ray,
                                  const std::vector<eig::Array3f> &verts,
                                  const std::vector<eig::Array3u> &elems) {
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
      guard_continue(n_dot_d < 0.f);

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

  // Given a ray object, find the nearest front-facing triangle intersecting the ray
  template <typename Traits>
  inline
  RayQuery ray_trace_nearest_elem(const Ray &ray,
                                  const TriMesh<Traits> &mesh) {
    RayQuery query;

    for (auto fh : mesh.faces()) {
      auto vh = fh.vertices().to_array<3>();
      eig::Vector3f a = to_eig<float, 3>(mesh.point(vh[0])), 
                    b = to_eig<float, 3>(mesh.point(vh[1])),
                    c = to_eig<float, 3>(mesh.point(vh[2]));

      // Compute edges, plane normal
      eig::Vector3f ab = b - a, bc = c - b, ca = a - c;
      eig::Vector3f n  = bc.cross(ab).normalized();
      
      // Test if intersected plane is front-facing
      float n_dot_d = n.dot(ray.d);
      guard_continue(n_dot_d < 0.f);

      // Test if intersection point is closer than current t
      float t = ((a + b + c) / 3.f - ray.o).dot(n) / n_dot_d;
      guard_continue(t >= 0.f && t < query.t);

      // Test if intersection point lies within triangle boundaries
      eig::Vector3f x = ray.o + t * ray.d;
      guard_continue(n.dot((x - a).cross(ab)) >= 0.f);
      guard_continue(n.dot((x - b).cross(bc)) >= 0.f);
      guard_continue(n.dot((x - c).cross(ca)) >= 0.f);

      query = { t, static_cast<uint>(fh.idx()) };
    }

    return query;
  }
} // namespace met