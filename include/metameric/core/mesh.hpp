#pragma once

#include <metameric/core/math.hpp>
#include <metameric/core/utility.hpp>
#include <metameric/core/detail/openmesh.hpp>
#include <span>
#include <vector>
#include <unordered_map>
#include <utility>

namespace met {
  /* An indexed mesh with face normals and no additional data */
  struct BaselineMeshTraits : public omesh::DefaultTraits {
    // Define default attributes; only face normals are stored, half-edges are intentionally omitted
    VertexAttributes(omesh::Attributes::None);
    HalfedgeAttributes(omesh::Attributes::None);
    FaceAttributes(omesh::Attributes::None);
  };

  /* An indexed mesh with face normals */
  struct FNormalMeshTraits : public omesh::DefaultTraits {
    VertexAttributes(omesh::Attributes::Status);
    HalfedgeAttributes(omesh::Attributes::None);
    FaceAttributes(omesh::Attributes::Normal | omesh::Attributes::Status);
  };

  /* An indexed mesh with face normals and half-edges to simplify a number of operations */
  struct HalfedgeMeshTraits : public omesh::DefaultTraits {
    VertexAttributes(omesh::Attributes::Status);
    HalfedgeAttributes(omesh::Attributes::PrevHalfedge | omesh::Attributes::Status);
    FaceAttributes(omesh::Attributes::Normal | omesh::Attributes::Status);
  };

  // Triangle mesh shorthands implementing the above defined traits
  template <typename Traits>
  using TriMesh      = omesh::TriMesh_ArrayKernelT<Traits>;
  using BaselineMesh = TriMesh<BaselineMeshTraits>;
  using FNormalMesh  = TriMesh<FNormalMeshTraits>;
  using HalfedgeMesh = TriMesh<HalfedgeMeshTraits>;

  /* Generational helper functions */

  template <typename Traits, typename T = eig::Array3f>
  std::pair<std::vector<T>, std::vector<eig::Array3u>> generate_data(const TriMesh<Traits> &mesh);

  template <typename Traits, typename T = eig::Array3f>
  TriMesh<Traits> generate_from_data(std::span<const T> vertices, std::span<const eig::Array3u> elements);

  template <typename Traits>
  TriMesh<Traits> generate_octahedron();

  template <typename Traits>
  TriMesh<Traits> generate_spheroid(uint n_subdivs = 3);

  template <typename Traits, typename T = eig::AlArray3f>
  TriMesh<Traits> generate_convex_hull(std::span<const T> points, const TriMesh<Traits> &spheroid_mesh = generate_spheroid<Traits>());

  template <typename Traits>
  TriMesh<Traits> simplify(const TriMesh<Traits> &mesh, uint max_vertices);

  // // FWD
  // template <typename T, typename E = eig::Array3u>
  // struct IndexedMesh;

  // /* Simple indexed triangle mesh structure */
  // template <typename T, typename E>
  // struct IndexedMesh {
  //   using Vert = T;
  //   using Elem = E;

  // private:
  //   std::vector<Vert> m_verts;
  //   std::vector<Elem> m_elems;

  // public:
  //   IndexedMesh() = default;
  //   IndexedMesh(std::span<const Vert> verts, std::span<const Elem> elems);

  //   // Accessors
  //   std::vector<Vert>& verts() { return m_verts; }
  //   std::vector<Elem>& elems() { return m_elems; }
  //   const std::vector<Vert>& verts() const { return m_verts; }
  //   const std::vector<Elem>& elems() const { return m_elems; }

  //   // Algorithms

  //   T centroid() const;
  // };

  // // Generate a subdivided octahedron whose vertices lie on a unit sphere
  // template <typename T = eig::AlArray3f>
  // IndexedMesh<T> generate_unit_sphere(uint n_subdivs = 3);

  // // Generate an approximate convex hull from a mesh describing a unit sphere
  // // by matching each vertex to a point
  // template <typename T = eig::AlArray3f>
  // IndexedMesh<T, eig::Array3u> generate_convex_hull(const IndexedMesh<T, eig::Array3u> &sphere_mesh, 
  //                                                   std::span<const T> points,
  //                                                   float threshold,
  //                                                   float max_error);

  // /* Convex hull functions */

  // // Shorthand that first generates a sphere mesh
  // template <typename T = eig::AlArray3f>
  // IndexedMesh<T, eig::Array3u> generate_convex_hull(std::span<const T> points);

  // template <typename T = eig::AlArray3f>
  // bool is_point_inside_convex_hull(const IndexedMesh<T, eig::Array3u> &chull, const T &test_point);

  // /* Miscellaneous functions */

  // // Generate a wireframe mesh from an input triangle mesh
  // template <typename T = eig::AlArray3f>
  // IndexedMesh<T, eig::Array2u> generate_wireframe(const IndexedMesh<T, eig::Array3u> &mesh);

  // // Perform progressive mesh simplification by edge collapse until vertex count <= max_vertices
  // template <typename T = eig::AlArray3f>
  // IndexedMesh<T, eig::Array3u> simplify_mesh(const IndexedMesh<T, eig::Array3u> &mesh, uint max_vertices);
} // namespace met