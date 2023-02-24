#pragma once

#include <metameric/core/math.hpp>
#include <metameric/core/utility.hpp>
#include <metameric/core/detail/openmesh.hpp>
#include <span>
#include <vector>
#include <unordered_map>
#include <utility>

namespace met {
  /* An indexed mesh with face normals and half-edges to simplify a number of operations */
  struct HalfedgeMeshTraits : public omesh::DefaultTraits {
    VertexAttributes(omesh::Attributes::Status);
    HalfedgeAttributes(omesh::Attributes::PrevHalfedge | omesh::Attributes::Status);
    FaceAttributes(omesh::Attributes::Normal | omesh::Attributes::Status);
  };

  // Triangle mesh shorthand for simplifications, using the openmesh halfedge implementation 
  template <typename Traits>
  using TriMesh          = omesh::TriMesh_ArrayKernelT<Traits>;
  using HalfedgeMeshData = TriMesh<HalfedgeMeshTraits>;

  // Simple, indexed triangle mesh representation
  using IndexedMeshData = std::pair<
    std::vector<eig::Array3f>,
    std::vector<eig::Array3u>
  >;

  // Simple, indexed triangle mesh representation with packed vec3 data (for OpenGL)
  using AlignedMeshData = std::pair<
    std::vector<eig::AlArray3f>,
    std::vector<eig::Array3u>
  >;

  // Convert between halfedge/indexed/aligned mesh data structures
  template <typename OutputMesh, typename InputMesh>
  OutputMesh convert_mesh(const InputMesh &mesh);

  /* Generational helper functions */

  // Returns a simple octahedral mesh, fitted inside a unit cube
  template <typename Mesh>
  Mesh generate_octahedron();
  
  // Returns a repeatedly subdivided spherical mesh, fitted inside a unit cube
  template <typename Mesh>
  Mesh generate_spheroid(uint n_subdivs = 3);

  // Returns a convex hull mesh around a set of points in 3D
  template <typename Mesh, typename Vector>
  Mesh generate_convex_hull(std::span<const Vector> data);

  /* Mesh simplification functions */

  // Performs progressive edge collapse for edges below max_edge_length
  template <typename OutputMesh, typename InputMesh>
  OutputMesh simplify_edge_length(const InputMesh &mesh, float max_edge_length = 0.f);

  // Performs volume-preserving progressive edge collapse until max_vertices remain; newly placed vertices
  // are optionally clipped into a secondary mesh optional_bounds
  template <typename OutputMesh, typename InputMesh>
  OutputMesh simplify_volume(const InputMesh &mesh, uint max_vertices, const InputMesh *optional_bounds = nullptr);
} // namespace met