#pragma once

#include <metameric/core/math.hpp>
#include <metameric/core/utility.hpp>
#include <metameric/core/detail/openmesh.hpp>
#include <array>
#include <span>
#include <vector>
#include <unordered_map>
#include <utility>

namespace met {
  /* Mesh formats */

  // Triangle mesh traits for the openmesh halfedge implementation 
  struct HalfedgeMeshTraits : public omesh::DefaultTraits {
    VertexAttributes(omesh::Attributes::Status);
    HalfedgeAttributes(omesh::Attributes::PrevHalfedge | omesh::Attributes::Status);
    FaceAttributes(omesh::Attributes::Normal | omesh::Attributes::Status);
  };

  // Triangle mesh shorthand for the openmesh halfedge implementation 
  using HalfedgeMeshData = omesh::TriMesh_ArrayKernelT<HalfedgeMeshTraits>;

  template <typename Vert, 
            typename Elem>
  struct TemplateMeshData {
    using VertTy = Vert;
    using ElemTy = Elem;
    using NormTy = Vert;
    using UVTy   = eig::Array2f;

  public:
    // Primary mesh data; must be available
    std::vector<VertTy> verts;
    std::vector<ElemTy> elems;
    
    // Secondary mesh data; might be available, should query
    std::vector<VertTy> norms;
    std::vector<UVTy>   uvs;

    // Data queries for secondary mesh data
    bool has_norms() const { return norms.size() == verts.size(); }
    bool has_uvs()   const { return uvs.size()   == verts.size(); }
  };
  
  using IndexedMeshData     = TemplateMeshData<eig::Array3f,   eig::Array3u>;
  using AlignedMeshData     = TemplateMeshData<eig::AlArray3f, eig::Array3u>;
  using IndexedDelaunayData = TemplateMeshData<eig::Array3f,   eig::Array4u>;
  using AlignedDelaunayData = TemplateMeshData<eig::AlArray3f, eig::Array4u>;

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

  // Returns a set of simplices representing a delaunay triangulation of a set of points in 3D
  template <typename Mesh, typename Vector>
  Mesh generate_delaunay(std::span<const Vector> data);

  /* Mesh simplification functions */

  // Performs progressive edge collapse for edges below max_edge_length
  template <typename OutputMesh, typename InputMesh>
  OutputMesh simplify_edge_length(const InputMesh &mesh, float max_edge_length = 0.f);

  // Performs volume-preserving progressive edge collapse until max_vertices remain; newly placed vertices
  // are optionally clipped into a secondary mesh optional_bounds
  template <typename OutputMesh, typename InputMesh>
  OutputMesh simplify_volume(const InputMesh &mesh, uint max_vertices, const InputMesh *optional_bounds = nullptr);
} // namespace met