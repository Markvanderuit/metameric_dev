#pragma once

#include <metameric/core/math.hpp>
#include <vector>

namespace met {
  /* Simple indexed triangle mesh structure */
  template <typename T>
  struct IndexedMesh {
    using Vert = T;
    using Elem = eig::Array3u;

    std::vector<Vert> vertices;
    std::vector<Elem> elements;
  };
  
  using Mesh = IndexedMesh<eig::AlArray3f>;

  Mesh generate_unit_sphere(uint n_subdivs = 3);
  Mesh generate_convex_hull(const Mesh &sphere_mesh,
                            const std::vector<eig::Array3f> &points);
  Mesh generate_convex_hull(const std::vector<eig::Array3f> &points);
} // namespace met