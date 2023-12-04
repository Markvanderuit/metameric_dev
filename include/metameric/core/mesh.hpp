#pragma once

#include <metameric/core/math.hpp>
#include <metameric/core/serialization.hpp>
#include <metameric/core/utility.hpp>
#include <array>
#include <span>
#include <vector>
#include <unordered_map>
#include <utility>

namespace met {
  // Simple indexed mesh representation with optional normal/texcoord data
  template <typename Vt, typename El>
  struct MeshBase {
    using vert_type = Vt;
    using elem_type = El;
    using norm_type = Vt;
    using txuv_type = eig::Array2f;

  public:
    // Primary mesh data; must be available
    std::vector<vert_type> verts;
    std::vector<elem_type> elems;
    
    // Secondary mesh data; might be available, should query
    std::vector<vert_type> norms;
    std::vector<txuv_type> txuvs;

    // Data queries for secondary mesh data, available per-vertex
    bool has_norms() const { return norms.size() == verts.size(); }
    bool has_txuvs() const { return txuvs.size() == verts.size(); }

  public: // Serialization
    void to_stream(std::ostream &str) const {
      met_trace();
      io::to_stream(verts, str);
      io::to_stream(elems, str);
      io::to_stream(norms, str);
      io::to_stream(txuvs, str);
    }

    void fr_stream(std::istream &str) {
      met_trace();
      io::fr_stream(verts, str);
      io::fr_stream(elems, str);
      io::fr_stream(norms, str);
      io::fr_stream(txuvs, str);
    }
  };
  
  // General mesh/delaunay types used throughout the application
  using Mesh       = MeshBase<eig::Array3f,   eig::Array3u>;
  using AlMesh     = MeshBase<eig::AlArray3f, eig::Array3u>;
  using Delaunay   = MeshBase<eig::Array3f,   eig::Array4u>;
  using AlDelaunay = MeshBase<eig::AlArray3f, eig::Array4u>;

  // Convert between indexed/aligned mesh data structures
  template <typename OutputMesh, typename InputMesh>
  OutputMesh convert_mesh(const InputMesh &mesh);

  /* Generational helper functions */

  // // Returns a simple octahedral mesh, fitted inside a unit cube
  // template <typename Mesh>
  // Mesh generate_octahedron();
  
  // // Returns a repeatedly subdivided spherical mesh, fitted inside a unit cube
  // template <typename Mesh>
  // Mesh generate_spheroid(uint n_subdivs = 3);

  // Returns a convex hull mesh around a set of points in 3D
  template <typename Mesh, typename Vector>
  Mesh generate_convex_hull(std::span<const Vector> data);

  // Returns a set of simplices representing a delaunay triangulation of a set of points in 3D
  template <typename Mesh, typename Vector>
  Mesh generate_delaunay(std::span<const Vector> data);
  
  template <typename OutputMesh, typename InputMesh>
  OutputMesh optimize_mesh(const InputMesh &mesh);

  template <typename OutputMesh, typename InputMesh>
  OutputMesh simplify_mesh(const InputMesh &mesh);

  /* Mesh simplification functions */

  // // Performs progressive edge collapse for edges below max_edge_length
  // template <typename OutputMesh, typename InputMesh>
  // OutputMesh simplify_edge_length(const InputMesh &mesh, float max_edge_length = 0.f);

  // // Performs progressive edge collapse until max_vertices remain; newly placed vertices
  // template <typename OutputMesh, typename InputMesh>
  // OutputMesh simplify_progressive(const InputMesh &mesh, uint max_vertices);

  // // Performs volume-preserving progressive edge collapse until max_vertices remain; newly placed vertices
  // // are optionally clipped into a secondary mesh optional_bounds
  // template <typename OutputMesh, typename InputMesh>
  // OutputMesh simplify_volume(const InputMesh &mesh, uint max_vertices, const InputMesh *optional_bounds = nullptr);
} // namespace met