#pragma once

#include <metameric/core/math.hpp>
#include <span>
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

  struct MeshTriangle {

  };

  template <typename T>
  struct SeparateMesh {
    
  };
  
  using Array3fMesh   = IndexedMesh<eig::Array3f>;
  using AlArray3fMesh = IndexedMesh<eig::AlArray3f>;

  // Generate a subdivided octahedron whose vertices lie on a unit sphere
  template <typename T = eig::AlArray3f>
  IndexedMesh<T> generate_unit_sphere(uint n_subdivs = 3);

  // Generate an approximate convex hull from a mesh describing a unit sphere
  // by matching each vertex to a point
  template <typename T = eig::AlArray3f>
  IndexedMesh<T> generate_convex_hull(const IndexedMesh<T> &sphere_mesh,
                                      std::span<const T> points);

  // Shorthand that first generates a sphere mesh
  template <typename T = eig::AlArray3f>
  IndexedMesh<T> generate_convex_hull(std::span<const T> points);

  template <typename T = eig::AlArray3f>
  IndexedMesh<T> simplify_mesh(const IndexedMesh<T> &mesh, uint max_vertices);
} // namespace met