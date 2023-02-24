#pragma once

#include <metameric/core/math.hpp>
#include <limits>

namespace met {
  // Simple ray tracing structure: origin, direction vectors
  struct Ray { eig::Vector3f o, d; };

  // Simple ray query object, returned on ray trace operation
  struct RayQuery {
    // Distance to intersected position
    float t = std::numeric_limits<float>::max();

    // Index of relevant query object
    uint i;

    // Validity querying
    bool is_valid() const { return t != std::numeric_limits<float>::max(); }
    explicit operator bool() const { return is_valid(); } 
  };

  // Ray trace against a mesh, returning a query of the nearest vertex within min_distance from any point along the ray
  template <typename Mesh>
  RayQuery raytrace_vert(const Ray &ray, const Mesh &mesh, float min_distance = 0.025f);

  // Ray trace against a mesh, returning a query of the nearest (front-facing) element along the ray
  template <typename Mesh>
  RayQuery raytrace_elem(const Ray &ray, const Mesh &mesh, bool cull_backface = true);
} // namespace met