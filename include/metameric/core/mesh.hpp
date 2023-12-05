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

  // Returns a convex hull mesh around a set of points in 3D
  template <typename Mesh, typename Vector>
  Mesh generate_convex_hull(std::span<const Vector> data);

  // Returns a set of simplices representing a delaunay triangulation of a set of points in 3D
  template <typename Mesh, typename Vector>
  Mesh generate_delaunay(std::span<const Vector> data);
  
  /* Modification functions */
  
  // Restructure mesh indexing to strip redundant vertices
  template <typename OutputMesh, typename InputMesh>
  OutputMesh remap_mesh(const InputMesh &mesh);
  
  // Restructure mesh indexing to strip unused vertices
  template <typename OutputMesh, typename InputMesh>
  OutputMesh compact_mesh(const InputMesh &mesh);

  // Run several Meshoptimizer methods that do not affect visual appearance
  template <typename OutputMesh, typename InputMesh>
  OutputMesh optimize_mesh(const InputMesh &mesh);

  // Run Meshoptimizer's simplify s.a. it does affect visual appearance but preserves topology
  template <typename OutputMesh, typename InputMesh>
  OutputMesh simplify_mesh(const InputMesh &mesh, uint target_elems, float target_error = std::numeric_limits<float>::max());

  // Run Meshoptimizer's simplify s.a. it does affect visual appearance and destroys topology
  template <typename OutputMesh, typename InputMesh>
  OutputMesh decimate_mesh(const InputMesh &mesh, uint target_elems, float target_error = std::numeric_limits<float>::max());
} // namespace met