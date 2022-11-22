#pragma once

#include <metameric/core/math.hpp>
#include <metameric/core/utility.hpp>
#include <span>
#include <vector>
#include <unordered_map>
#include <utility>

namespace met {
  // FWD
  template <typename T, typename E = eig::Array3u>
  struct IndexedMesh;
  template <typename T>
  struct HalfEdgeMesh;

  /* Simple indexed triangle mesh structure */
  template <typename T, typename E>
  struct IndexedMesh {
    using Vert = T;
    using Elem = E;

  private:
    std::vector<Vert> m_verts;
    std::vector<Elem> m_elems;

  public:
    IndexedMesh() = default;
    IndexedMesh(const HalfEdgeMesh<T> &other);
    IndexedMesh(std::span<const Vert> verts, std::span<const Elem> elems);

    // Accessors
    std::vector<Vert>& verts() { return m_verts; }
    std::vector<Elem>& elems() { return m_elems; }
    const std::vector<Vert>& verts() const { return m_verts; }
    const std::vector<Elem>& elems() const { return m_elems; }
  };

  template <typename T = eig::AlArray3f>
  struct HalfEdgeMesh {
    struct Vert {
      T p;         // Position vector
      uint half_i; // Component index
    };
    
    struct Face {
      uint half_i; // Component index
    };

    struct Half {
      uint twin_i, next_i, prev_i; // Half-edge indexes
      uint vert_i, face_i;         // Component indices
    };

  private:
    std::vector<Vert> m_verts;
    std::vector<Face> m_faces;
    std::vector<Half> m_halfs;

  public:
    HalfEdgeMesh(const IndexedMesh<T> other);

    // Accessors
    std::vector<Vert>& verts() { return m_verts; }
    std::vector<Face>& faces() { return m_faces; }
    std::vector<Half>& halfs() { return m_halfs; }
    const std::vector<Vert>& verts() const { return m_verts; }
    const std::vector<Face>& faces() const { return m_faces; }
    const std::vector<Half>& halfs() const { return m_halfs; }

    // Surrounding accessors
    std::vector<uint> verts_around_vert(uint vert_i) const;
    std::vector<uint> halfs_storing_vert(uint vert_i) const;
    std::vector<uint> verts_around_face(uint face_i) const;
    std::vector<uint> halfs_around_face(uint face_i) const;
    std::vector<uint> faces_around_vert(uint vert_i) const;
    std::vector<uint> faces_around_half(uint half_i) const;
    std::vector<uint> faces_around_face(uint face_i) const;
  };
  
  using Array3fMesh        = IndexedMesh<eig::Array3f, eig::Array3u>;
  using AlArray3fMesh      = IndexedMesh<eig::AlArray3f, eig::Array3u>;
  using Array3fWireframe   = IndexedMesh<eig::Array3f, eig::Array2u>;
  using AlArray3fWireframe = IndexedMesh<eig::AlArray3f, eig::Array2u>;

  // Generate a subdivided octahedron whose vertices lie on a unit sphere
  template <typename T = eig::AlArray3f>
  IndexedMesh<T> generate_unit_sphere(uint n_subdivs = 3);

  // Generate an approximate convex hull from a mesh describing a unit sphere
  // by matching each vertex to a point
  template <typename T = eig::AlArray3f>
  IndexedMesh<T, eig::Array3u> generate_convex_hull(const IndexedMesh<T, eig::Array3u> &sphere_mesh, 
                                                    std::span<const T> points);

  /* Mesh cleanup functions */

  template <typename T = eig::AlArray3f>
  void clean_stitch_vertices(IndexedMesh<T, eig::Array3u> &mesh);
  template <typename T = eig::AlArray3f>
  void clean_delete_unused_vertices(IndexedMesh<T, eig::Array3u> &mesh);
  template <typename T = eig::AlArray3f>
  void clean_delete_double_elems(IndexedMesh<T, eig::Array3u> &mesh);
  template <typename T = eig::AlArray3f>
  void clean_delete_collapsed_elems(IndexedMesh<T, eig::Array3u> &mesh);
  template <typename T = eig::AlArray3f>
  void clean_fix_winding_order(IndexedMesh<T, eig::Array3u> &mesh);
  template <typename T = eig::AlArray3f>
  void clean_all(IndexedMesh<T, eig::Array3u> &mesh);

  /* Convex hull functions */

  // Shorthand that first generates a sphere mesh
  template <typename T = eig::AlArray3f>
  IndexedMesh<T, eig::Array3u> generate_convex_hull(std::span<const T> points);

  template <typename T = eig::AlArray3f>
  T move_point_inside_convex_hull(const IndexedMesh<T, eig::Array3u> &chull, const T &test_point);

  /* Miscellaneous functions */

  // Generate a wireframe mesh from an input triangle mesh
  template <typename T = eig::AlArray3f>
  IndexedMesh<T, eig::Array2u> generate_wireframe(const IndexedMesh<T, eig::Array3u> &mesh);

  // Perform progressive mesh simplification by edge collapse until vertex count <= max_vertices
  template <typename T = eig::AlArray3f>
  HalfEdgeMesh<T> simplify_mesh(const HalfEdgeMesh<T> &mesh, uint max_vertices);
  template <typename T = eig::AlArray3f>
  IndexedMesh<T, eig::Array3u> simplify_mesh(const IndexedMesh<T, eig::Array3u> &mesh, uint max_vertices);
} // namespace met