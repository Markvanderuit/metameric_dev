#pragma once

#include <metameric/core/fwd.hpp>
#include <metameric/core/serialization.hpp>
#include <metameric/core/utility.hpp>

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
    bool empty()     const { return verts.empty() || elems.empty(); }
    bool has_norms() const { return !norms.empty(); }
    bool has_txuvs() const { return !txuvs.empty(); }

  public: // Serialization
    void to_stream(std::ostream &str) const {
      met_trace();
      io::to_stream(verts, str);
      io::to_stream(elems, str);
      io::to_stream(norms, str);
      io::to_stream(txuvs, str);
    }

    void from_stream(std::istream &str) {
      met_trace();
      io::from_stream(verts, str);
      io::from_stream(elems, str);
      io::from_stream(norms, str);
      io::from_stream(txuvs, str);
    }
  };
  
  // General mesh/delaunay types used throughout the application
  using Mesh       = MeshBase<eig::Array3f,   eig::Array3u>;
  using AlMesh     = MeshBase<eig::AlArray3f, eig::Array3u>;
  using Delaunay   = MeshBase<eig::Array3f,   eig::Array4u>;
  using AlDelaunay = MeshBase<eig::AlArray3f, eig::Array4u>;

  /* Generational helper functions */

  // Returns a convex hull mesh around a set of points in 3D
  template <typename Mesh, typename Vector>
  Mesh generate_convex_hull(std::span<const Vector> data);

  // Returns a set of simplices representing a delaunay triangulation of a set of points in 3D
  // and writes a list of integer indices to scratch_list, of deleted vertices
  template <typename Mesh, typename Vector>
  Mesh generate_delaunay(std::span<const Vector> data);
  
  /* In-place modification functions */
  
  // (Re)compute smooth vertex normals from scratch
  template <typename MeshTy>
  void renormalize_mesh(MeshTy &mesh);
  
  // Restructure mesh indexing to strip redundant/unused elements
  template <typename MeshTy>
  void remap_mesh(MeshTy &mesh);
  
  // Restructure mesh indexing to strip redundant/unused vertices
  template <typename MeshTy>
  void compact_mesh(MeshTy &mesh);

  // Run several Meshoptimizer methods that do not affect visual appearance
  template <typename MeshTy>
  void optimize_mesh(MeshTy &mesh);

  // Run Meshoptimizer's simplify, affecting visual appearance but preserving topology
  template <typename MeshTy>
  void simplify_mesh(MeshTy &mesh, uint target_elems, float target_error = std::numeric_limits<float>::max());

  // Run Meshoptimizer's simpliy,  affecting visual appearance and foregoeing topology
  template <typename MeshTy>
  void decimate_mesh(MeshTy &mesh, uint target_elems, float target_error = std::numeric_limits<float>::max());

  // Generate unique texture coordinates for a currently uv-mapped mesh
  /* template <typename MeshTy>
  std::vector<typename MeshTy::txuv_type> parameterize_mesh(MeshTy &mesh); */

  // Adjust a mesh s.t. the entire shape fits within [0, 1], and return
  // a transform to invert the operation
  template <typename MeshTy>
  eig::Matrix4f unitize_mesh(MeshTy &mesh);

  // Adjust a mesh so there are no triangles with 0-size UVs
  template <typename MeshTy>
  void fix_degenerate_uvs(MeshTy &mesh);

  /* Copying modification functions */
  
  // Convert between indexed/aligned/other mesh types
  template <typename OutputMesh, typename InputMesh>
  OutputMesh convert_mesh(const InputMesh &mesh);

  // Restructure mesh indexing to strip redundant vertices
  template <typename OutputMesh, typename InputMesh>
  OutputMesh remapped_mesh(const InputMesh &mesh) {
    met_trace();
    auto copy = convert_mesh<OutputMesh>(mesh);
    remap_mesh(copy);
    return copy;
  }
  
  // Restructure mesh indexing to strip unused vertices
  template <typename OutputMesh, typename InputMesh>
  OutputMesh compacted_mesh(const InputMesh &mesh) {
    met_trace();
    auto copy = convert_mesh<OutputMesh>(mesh);
    compact_mesh(copy);
    return copy;
  }

  // Run several Meshoptimizer methods that do not affect visual appearance
  template <typename OutputMesh, typename InputMesh>
  OutputMesh optimized_mesh(const InputMesh &mesh) {
    met_trace();
    auto copy = convert_mesh<OutputMesh>(mesh);
    optimize_mesh(copy);
    return copy;
  }

  // (Re)compute vertex normals from scratch
  template <typename OutputMesh, typename InputMesh>
  OutputMesh renormalized_mesh(const InputMesh &mesh) {
    met_trace();
    auto copy = convert_mesh<OutputMesh>(mesh);
    renormalize_mesh(copy);
    return copy;
  }

  // Slightly adjust collapsed or degenerate UV coordinates
  template <typename OutputMesh, typename InputMesh>
  OutputMesh fixed_degenerate_uvs(const InputMesh &mesh) {
    met_trace();
    auto copy = convert_mesh<OutputMesh>(mesh);
    fix_degenerate_uvs(copy);
    return copy;
  }

  // Run Meshoptimizer's simplify s.a. it does affect visual appearance but preserves topology
  template <typename OutputMesh, typename InputMesh>
  OutputMesh simplified_mesh(const InputMesh &mesh, uint target_elems, float target_error = std::numeric_limits<float>::max()) {
    met_trace();
    auto copy = convert_mesh<OutputMesh>(mesh);
    simplify_mesh(copy, target_elems, target_error);
    return copy;
  }

  // Run Meshoptimizer's simplify s.a. it does affect visual appearance and destroys topology
  template <typename OutputMesh, typename InputMesh>
  OutputMesh decimated_mesh(const InputMesh &mesh, uint target_elems, float target_error = std::numeric_limits<float>::max()) {
    met_trace();
    auto copy = convert_mesh<OutputMesh>(mesh);
    decimate_mesh(copy, target_elems, target_error);
    return copy;
  }

  /* // Parameterize a mesh s.t. uv coordinates are unique
  template <typename OutputMesh, typename InputMesh>
  std::pair<OutputMesh, std::vector<typename OutputMesh::txuv_type>> parameterized_mesh(const InputMesh &mesh) {
    met_trace();
    auto copy = convert_mesh<OutputMesh>(mesh);
    auto txuv = parameterize_mesh(copy);
    return { copy, txuv };
  } */

  // Unitize a mesh s.t. it sits in a [0, 1] cube, and return mesh and inverse transform
  template <typename OutputMesh, typename InputMesh>
  std::pair<OutputMesh, eig::Matrix4f> unitized_mesh(const InputMesh &mesh) {
    met_trace();
    auto copy = convert_mesh<OutputMesh>(mesh);
    auto trnf = unitize_mesh(copy);
    return { copy, trnf };
  }

  /* Define pre-included mesh data */
  namespace models {
    extern Mesh unit_rect;
  } // namespace models
} // namespace met