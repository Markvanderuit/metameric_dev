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

  template <typename Traits>
  TriMesh<Traits> simplify(const TriMesh<Traits> &mesh, 
                           const TriMesh<Traits> &bounds,
                           uint max_vertices);

  template <typename Traits>
  TriMesh<Traits> simplify_edges(const TriMesh<Traits> &mesh, float max_edge_length);

  template <typename Traits, typename T = eig::AlArray3f>
  TriMesh<Traits> generate_convex_hull(std::span<const T> points);
  
  template <typename T = eig::AlArray3f>
  std::pair<std::vector<T>, std::vector<eig::Array3u>> generate_convex_hull(std::span<const T> points);

  template <typename Traits, typename T = eig::AlArray3f>
  TriMesh<Traits> generate_convex_hull_approx(std::span<const T> points, 
    const TriMesh<Traits> &spheroid_mesh = generate_spheroid<Traits>());
} // namespace met