#pragma once

#include <metameric/core/math.hpp>
#include <metameric/core/utility.hpp>
#include <span>
#include <vector>
#include <unordered_map>
#include <utility>

namespace met {
  /* Simple indexed triangle mesh structure */
  template <typename T>
  struct IndexedMesh {
    using Vert = T;
    using Elem = eig::Array3u;

    std::vector<Vert> vertices;
    std::vector<Elem> elements;
  };

  template <typename T = eig::AlArray3f>
  struct SimpleMesh {
    struct Face {
      T x, y, z;
    };

    private:
      std::vector<Face> m_faces;

    public:

  };

  template <typename T = eig::AlArray3f>
  struct HalfEdgeMesh {
    struct Vertex {
      T p;         // Position vector
      uint half_i; // Component index
    };
    
    struct Face {
      uint half_i; // Component index
    };

    struct HalfEdge {
      uint twin_i, next_i, prev_i; // Half-edge indexes
      uint vert_i, face_i;         // Component indices
    };

  private:
    std::vector<Vertex>   m_vertices;
    std::vector<Face>     m_faces;
    std::vector<HalfEdge> m_halves;

  public:
    HalfEdgeMesh(const IndexedMesh<T> other);

    // Index accessors
    std::vector<uint> vertices_for_face(uint face_i);
    std::vector<uint> halves_for_face(uint face_i);
    std::vector<uint> faces_around_vertex(uint vert_i);
    std::vector<uint> faces_around_face(uint face_i);
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