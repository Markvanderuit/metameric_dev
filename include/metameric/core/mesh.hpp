#pragma once

#include <metameric/core/math.hpp>

namespace met {
  /* Simple indexed mesh structure */
  struct Mesh {
    using VertType = eig::AlArray3f;
    using ElemType = eig::Array3u;

    std::vector<VertType> vertices;
    std::vector<ElemType> elements; 
  };

  Mesh generate_unit_sphere(uint n_subdivs = 3);
  Mesh generate_convex_hull(const Mesh &sphere_mesh,
                            const std::vector<eig::Array3f> &points);
  Mesh generate_convex_hull(const std::vector<eig::Array3f> &points);
} // namespace met