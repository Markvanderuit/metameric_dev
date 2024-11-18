#include <metameric/core/mesh.hpp>
#include <metameric/core/ranges.hpp>
#include <metameric/core/utility.hpp>
#include <metameric/core/detail/trace.hpp>
#include <xatlas.h>
#include <meshoptimizer.h>
#include <libqhullcpp/Qhull.h>
#include <libqhullcpp/QhullVertexSet.h>
#include <libqhullcpp/QhullPoints.h>
#include <algorithm>
#include <execution>

namespace met {
  namespace detail {
    template <typename MeshTy>
    void grow_mesh(MeshTy &mesh, size_t n_verts) {
      mesh.verts.resize(n_verts);
      if (mesh.has_norms())
        mesh.norms.resize(n_verts);
      if (mesh.has_txuvs())
        mesh.txuvs.resize(n_verts);
    }

    template <typename MeshTy>
    void shrink_mesh_to_fit(MeshTy &mesh, size_t n_verts) {
      mesh.verts.resize(n_verts);
      mesh.verts.shrink_to_fit();
      if (mesh.has_norms()) {
        mesh.norms.resize(n_verts);
        mesh.norms.shrink_to_fit();
      }
      if (mesh.has_txuvs()) {
        mesh.txuvs.resize(n_verts);
        mesh.txuvs.shrink_to_fit();
      }
    }
  }

  namespace models {
    Mesh unit_rect = {
      .verts = {
        eig::Array3f(-1, -1, 0),
        eig::Array3f( 1, -1, 0),
        eig::Array3f(-1,  1, 0),
        eig::Array3f( 1,  1, 0)
      },
      .elems = {
        eig::Array3u(0, 1, 2),
        eig::Array3u(1, 3, 2)
      },
      .norms = {
        eig::Array3f(0, 0, 1),
        eig::Array3f(0, 0, 1),
        eig::Array3f(0, 0, 1),
        eig::Array3f(0, 0, 1)
      },
      .txuvs = {
        eig::Array2f(0, 0),
        eig::Array2f(1, 0),
        eig::Array2f(0, 1),
        eig::Array2f(1, 1)
      }
    };
  } // namespace models

  template <typename OutputMesh, typename InputMesh>
  OutputMesh convert_mesh(const InputMesh &mesh) requires std::same_as<OutputMesh, InputMesh> {
    met_trace_n("Passthrough");
    return mesh;
  }
  
  template <>
  Mesh convert_mesh<Mesh, AlMesh>(const AlMesh &mesh) {
    met_trace_n("AlMeshData -> Mesh");
    return { 
      .verts = std::vector<eig::Array3f>(range_iter(mesh.verts)), 
      .elems = mesh.elems,
      .norms = std::vector<eig::Array3f>(range_iter(mesh.norms)),
      .txuvs = mesh.txuvs 
    };
  }

  template <>
  AlMesh convert_mesh<AlMesh, Mesh>(const Mesh &mesh) {
    met_trace_n("Mesh -> AlMeshData");
    return { 
      .verts = std::vector<eig::AlArray3f>(range_iter(mesh.verts)), 
      .elems = mesh.elems,
      .norms = std::vector<eig::AlArray3f>(range_iter(mesh.norms)),
      .txuvs = mesh.txuvs 
    };
  }

  template <>
  Mesh convert_mesh<Mesh, Delaunay>(const Delaunay &mesh) {
    met_trace_n("Delaunay -> Mesh");
    
    std::vector<eig::Array3u> elems(4 * mesh.elems.size());

    #pragma omp parallel for
    for (int i = 0; i < mesh.elems.size(); ++i) {
      eig::Array4u in = mesh.elems[i];
      std::array<eig::Array3u, 4> out = { eig::Array3u { in[2], in[1], in[0] }, eig::Array3u { in[3], in[1], in[2] },
                                          eig::Array3u { in[3], in[2], in[0] }, eig::Array3u { in[3], in[0], in[1] } };
      std::ranges::copy(out, elems.begin() + 4 *  i);
    }

    return { mesh.verts, elems };
  }

  template <>
  Delaunay convert_mesh<Delaunay, AlDelaunay>(const AlDelaunay &mesh) {
    met_trace_n("AlDelaunayData -> Delaunay");
    return { std::vector<eig::Array3f>(range_iter(mesh.verts)), mesh.elems };
  }

  template <>
  AlDelaunay convert_mesh<AlDelaunay, Delaunay>(const Delaunay &mesh) {
    met_trace_n("Delaunay -> AlDelaunayData");
    return { std::vector<eig::AlArray3f>(range_iter(mesh.verts)), mesh.elems };
  }

  template <>
  AlMesh convert_mesh<AlMesh, Delaunay>(const Delaunay &mesh) {
    met_trace_n("Delaunay -> AlMeshData");
    return convert_mesh<AlMesh>(convert_mesh<Mesh>(mesh));
  }

  template <>
  AlMesh convert_mesh<AlMesh, AlDelaunay>(const AlDelaunay &mesh) {
    met_trace_n("AlDelaunayData -> AlMeshData");
    return convert_mesh<AlMesh>(convert_mesh<Delaunay>(mesh));
  }

  template <typename MeshTy, typename Vector>
  MeshTy generate_convex_hull(std::span<const Vector> data) {
    met_trace();

    std::vector<eig::Array3d> input(data.size());
    std::transform(std::execution::par_unseq, range_iter(data), input.begin(),
      [](const auto &v) { return v.template cast<double>().eval(); }); 

    // Query qhull for a convex hull structure
    auto qhull = orgQhull::Qhull("", 3, input.size(), cnt_span<const double>(input).data(), "Qt Qx C-0");
    qhull.setErrorStream(&std::cout);
      
    auto qh_verts = qhull.vertexList().toStdVector();
    auto qh_elems = qhull.facetList().toStdVector();

    // Assign incremental IDs to qh_verts; Qhull does not seem to manage removed vertices properly?
    #pragma omp parallel for
    for (int i = 0; i < qh_verts.size(); ++i) 
      qh_verts[i].getVertexT()->id = i;

    // Assemble indexed mesh data from qhull format
    std::vector<eig::Array3f> verts(qh_verts.size());
    std::vector<eig::Array3u> elems(qh_elems.size());
    std::transform(std::execution::par_unseq, range_iter(qh_elems), elems.begin(), [&](const auto &el) {
      eig::Array3u el_;
      std::ranges::transform(el.vertices().toStdVector(), el_.begin(), 
        [](const auto &v) { return v.id(); });
      return el_;
    });
    std::transform(std::execution::par_unseq, range_iter(qh_verts), verts.begin(), 
      [](const orgQhull::QhullVertex &vt) { 
        eig::Array3d v(vt.point().constBegin());
        return v.cast<float>().eval();
      });

    // Handle flipped triangles; Qhull sorts vertices by index, but we need consistent CW orientation 
    // to have outward normals for culled rendering; not to mention potential mesh simplification
    eig::Vector3f chull_cntr = std::reduce(std::execution::par_unseq, range_iter(verts), eig::Array3f(0)) 
                             / static_cast<float>(verts.size());
    std::for_each(std::execution::par_unseq, range_iter(elems), [&](auto &el) {
      eig::Vector3f a = verts[el[0]], b = verts[el[1]], c = verts[el[2]];
      eig::Vector3f norm = (b - a).cross(c - a).normalized().eval();
      eig::Vector3f cntr = ((a + b + c) / 3.f).eval();
      if (norm.dot((cntr - chull_cntr).normalized()) <= 0.f)
        el = eig::Array3u { el[2], el[1], el[0] };
    }); 

    return convert_mesh<MeshTy>(Mesh { verts, elems });
  }
  
  template <typename MeshTy, typename Vector>
  MeshTy generate_delaunay(std::span<const Vector> data) {
    met_trace();

    std::vector<eig::Array3d> input(data.size());
    std::transform(std::execution::par_unseq, range_iter(data), input.begin(),
      [](const auto &v) { return v.template cast<double>().eval(); }); 
    
    auto qhull = orgQhull::Qhull("", 3, input.size(), cnt_span<const double>(input).data(), "d Qbb Qt");
    qhull.setErrorStream(&std::cout);

    auto qh_verts = qhull.vertexList().toStdVector();
    auto qh_elems = qhull.facetList().toStdVector();

    // Assign incremental IDs to qh_verts; Qhull does not seem to manage removed vertices properly?
    #pragma omp parallel for
    for (int i = 0; i < qh_verts.size(); ++i) 
      qh_verts[i].getVertexT()->id = i;

    // Assemble indexed data from qhull format
    std::vector<eig::Array3f> verts(qh_verts.size());
    std::transform(std::execution::par_unseq, range_iter(qh_verts), verts.begin(), 
    [](const orgQhull::QhullVertex &vt) { 
      eig::Array3d v(vt.point().constBegin());
      return v.cast<float>().eval();
    });

    // Undo QHull's unnecessary scatter-because-screw-you-aaaaaargh
    std::vector<uint> vertex_idx(qh_verts.size());
    #pragma omp parallel for
    for (int i = 0; i < verts.size(); ++i) {
      auto it = rng::find_if(data, [&](const auto &v) { return v.isApprox(verts[i]); });
      if (it != data.end())
        vertex_idx[i] = std::distance(data.begin(), it);
    }
    
    // Build element data
    std::vector<eig::Array4u> elems(qh_elems.size());
    std::transform(std::execution::par_unseq, range_iter(qh_elems), elems.begin(), [&](const auto &el) {
      eig::Array4u el_;
      rng::transform(el.vertices().toStdVector(), el_.begin(), [&](const auto &v) { return vertex_idx[v.id()]; });
      return el_;
    });

    return convert_mesh<MeshTy, Delaunay>(Delaunay { std::vector<eig::Array3f>(range_iter(data)), elems });
  }

  template <typename MeshTy>
  void renormalize_mesh(MeshTy &mesh) {
    met_trace();

    // Reset all normal data to 0
    mesh.norms.clear();
    mesh.norms.resize(mesh.verts.size(), typename MeshTy::norm_type(0));

    // Generate unnormalized face vectors and add to output normals
    for (const auto &el : mesh.elems) {
      eig::Vector3f a = mesh.verts[el[0]], 
                    b = mesh.verts[el[1]], 
                    c = mesh.verts[el[2]];
      eig::Array3f n = (b - a).cross(c - a);
      mesh.norms[el[0]] += n;
      mesh.norms[el[1]] += n;
      mesh.norms[el[2]] += n;
    }

    // Normalize output 
    std::transform(std::execution::par_unseq, range_iter(mesh.norms), mesh.norms.begin(), 
      [](const auto &n) { return n.matrix().normalized().eval(); });
  }
  
  template <typename MeshTy>
  void remap_mesh(MeshTy &mesh) {
    met_trace();

    // Instantiate temporary buffer used for remapping operation, and
    // obtain a per-index view over mesh elements
    auto dst = std::vector<uint>(mesh.verts.size() * 3);
    auto src = cnt_span<const uint>(mesh.elems);
    
    // First, set up all mesh attribute streams that need remapping
    std::vector<meshopt_Stream> attribs;
    attribs.push_back({ mesh.verts.data(), sizeof(eig::Array3f), sizeof(typename MeshTy::vert_type) });
    if (mesh.has_norms())
      attribs.push_back({ mesh.norms.data(), sizeof(eig::Array3f), sizeof(typename MeshTy::norm_type) });
    if (mesh.has_txuvs())
      attribs.push_back({ mesh.txuvs.data(), sizeof(eig::Array2f), sizeof(typename MeshTy::txuv_type) });

    // Generate remap buffer
    size_t n_verts = meshopt_generateVertexRemapMulti(
      dst.data(), src.data(), src.size(), mesh.verts.size(), attribs.data(), attribs.size());

    // Expand mesh data if necessary
    detail::grow_mesh(mesh, std::max(n_verts, mesh.verts.size()));

    // Given remap buffer, remap all data in the mesh in-place
    meshopt_remapIndexBuffer(cnt_span<uint>(mesh.elems).data(), src.data(), src.size(), dst.data());
    meshopt_remapVertexBuffer(mesh.verts.data(), mesh.verts.data(), mesh.verts.size(), 
      sizeof(typename MeshTy::vert_type), dst.data());
    if (mesh.has_norms())
      meshopt_remapVertexBuffer(mesh.norms.data(), mesh.norms.data(), mesh.norms.size(), 
        sizeof(typename MeshTy::norm_type), dst.data());
    if (mesh.has_txuvs())
      meshopt_remapVertexBuffer(mesh.txuvs.data(), mesh.txuvs.data(), mesh.txuvs.size(), 
        sizeof(typename MeshTy::txuv_type), dst.data());
      
    // Shrink-to-fit mesh data if possible
    detail::shrink_mesh_to_fit(mesh, n_verts);
  }
  
  template <typename MeshTy>
  void compact_mesh(MeshTy &mesh) {
    met_trace();

    // Instantiate temporary buffer used for remapping operation, and
    // obtain a per-index view over mesh elements
    auto dst = std::vector<uint>(mesh.verts.size() * 3);
    auto src = cnt_span<const uint>(mesh.elems);
    
    // Generate remap buffer
    size_t n_verts = 
      meshopt_optimizeVertexFetchRemap(dst.data(), src.data(), src.size(), mesh.verts.size());

    // Expand mesh data if necessary
    detail::grow_mesh(mesh, std::max(n_verts, mesh.verts.size()));

    // Given remap buffer, remap all data in the mesh in-place
    meshopt_remapIndexBuffer(cnt_span<uint>(mesh.elems).data(), src.data(), src.size(), dst.data());
    meshopt_remapVertexBuffer(mesh.verts.data(), mesh.verts.data(), mesh.verts.size(), 
      sizeof(typename MeshTy::vert_type), dst.data());
    if (mesh.has_norms())
      meshopt_remapVertexBuffer(mesh.norms.data(), mesh.norms.data(), mesh.norms.size(), 
        sizeof(typename MeshTy::norm_type), dst.data());
    if (mesh.has_txuvs())
      meshopt_remapVertexBuffer(mesh.txuvs.data(), mesh.txuvs.data(), mesh.txuvs.size(), 
        sizeof(typename MeshTy::txuv_type), dst.data());

    // Shrink-to-fit mesh data if possible
    detail::shrink_mesh_to_fit(mesh, n_verts);
  }

  
  template <typename MeshTy>
  void optimize_mesh(MeshTy &mesh) {
    met_trace();

    // TODO implement ...
  }
  
  template <typename MeshTy>
  void simplify_mesh(MeshTy &mesh, uint target_elems, float target_error) {
    met_trace();

    guard(target_elems < mesh.elems.size());

    // Generate output/input ranges
    auto dst = std::vector<eig::Array3u>(mesh.elems.size());
    auto src = cnt_span<uint>(mesh.elems);

    // Run meshoptimizer's simplify in-place
    using vert_type = typename MeshTy::vert_type;
    size_t n_elems = meshopt_simplify(
      src.data(), src.data(), src.size(), 
      mesh.verts[0].data(), mesh.verts.size(), sizeof(vert_type),
      target_elems * 3, target_error, 0, nullptr) / 3;

    // Compact resulting mesh
    mesh.elems.resize(n_elems);
    mesh.elems.shrink_to_fit();
    compact_mesh(mesh);
  }

  template <typename MeshTy>
  void decimate_mesh(MeshTy &mesh, uint target_elems, float target_error) {
    met_trace();

    // Generate output/input ranges
    auto dst = std::vector<eig::Array3u>(mesh.elems.size());
    auto src = cnt_span<uint>(mesh.elems);
    
    // Run meshoptimizer's simplifySloppy in-place
    using vert_type = typename MeshTy::vert_type;
    size_t n_elems = meshopt_simplifySloppy(
      src.data(), src.data(), src.size(), 
      mesh.verts[0].data(), mesh.verts.size(), sizeof(vert_type),
      target_elems * 3, target_error, nullptr) / 3;

    // Compact resulting mesh
    mesh.elems.resize(n_elems);
    mesh.elems.shrink_to_fit();
    compact_mesh(mesh);
  }

  
  template <typename MeshTy>
  std::vector<typename MeshTy::txuv_type> parameterize_mesh(MeshTy &mesh) {
    met_trace();

    // Instantiate empty objects
    xatlas::Atlas *atlas = xatlas::Create();
    xatlas::MeshDecl mesh_decl;

    // Add mesh vertex data
    mesh_decl.vertexCount          = mesh.verts.size();
    mesh_decl.vertexPositionData   = reinterpret_cast<float *>(mesh.verts.data());
    mesh_decl.vertexPositionStride = sizeof(typename MeshTy::vert_type);

    // Add mesh normal data
    if (mesh.has_norms()) {
      mesh_decl.vertexNormalData   = reinterpret_cast<float *>(mesh.norms.data());
      mesh_decl.vertexNormalStride = sizeof(typename MeshTy::norm_type);
    }

    // Add mesh texcoord data
    if (mesh.has_txuvs()) {
      mesh_decl.vertexUvData   = reinterpret_cast<float *>(mesh.txuvs.data());
      mesh_decl.vertexUvStride = sizeof(typename MeshTy::txuv_type);
    }

    // Add mesh element data
    mesh_decl.indexCount  = mesh.elems.size() * MeshTy::elem_type::RowsAtCompileTime;
    mesh_decl.indexData   = reinterpret_cast<uint *>(mesh.elems.data());
    mesh_decl.indexFormat = xatlas::IndexFormat::UInt32;

    // Forward mesh data to atlas
    auto err = xatlas::AddMesh(atlas, mesh_decl, 1);
    debug::check_expr(!err, fmt::format("xatlas::Addmesh(...) returned error code {}", static_cast<uint>(err)));   

    // Finally, generate atlas
    xatlas::Generate(atlas, {}, {
      .bilinear   = true,
      .blockAlign = true,
      .padding    = 4,   // pixel padding, outside of bilinear padding
      // .texelsPerUnit = 1024.f,
      // .resolution = 1024 // Estimated texture size
    });
    const auto &parm = atlas->meshes[0];

    // Return value; old UVs are translated to new mesh and returned separately
    std::vector<typename MeshTy::txuv_type> old_txuvs;

    // Create appropriately sized placeholder mesh
    MeshTy mesh_;
    mesh_.verts.resize(parm.vertexCount);
    mesh_.elems.resize(parm.indexCount / 3);
    if (mesh.has_norms())
      mesh_.norms.resize(parm.vertexCount);
    if (mesh.has_txuvs()) {
      mesh_.txuvs.resize(parm.vertexCount);
      old_txuvs.resize(parm.vertexCount);
    }

    // Process atlas vertex data
    #pragma omp parallel for
    for (int i = 0; i < parm.vertexCount; ++i) {
      const auto &vt = parm.vertexArray[i];
      mesh_.verts[i] = mesh.verts[vt.xref];
      if (mesh.has_norms())
        mesh_.norms[i] = mesh.norms[vt.xref];
      if (mesh.has_txuvs()) {
        mesh_.txuvs[i] = { vt.uv[0] / atlas->width, 
                           vt.uv[1] / atlas->height };
        old_txuvs[i]   = mesh.txuvs[vt.xref];
      }
    }

    // Copy over atlas index data
    rng::copy(cast_span<typename MeshTy::elem_type>(std::span { parm.indexArray, parm.indexCount }), mesh_.elems.begin());

    // Cleanup objects
    xatlas::Destroy(atlas);

    // Input mesh becomes placeholder mesh, and old (remapped) UVs are returned
    mesh = mesh_;
    return old_txuvs;
  }
  
  template <typename MeshTy>
  eig::Matrix4f unitize_mesh(MeshTy &mesh) {
    met_trace();

    // Establish bounding box around mesh's vertices
    eig::Array3f minb = std::reduce(std::execution::par_unseq,
                                    range_iter(mesh.verts),
                                    mesh.verts[0],
                                    [](auto a, auto b) { return a.cwiseMin(b).eval(); });
    eig::Array3f maxb = std::reduce(std::execution::par_unseq,
                                    range_iter(mesh.verts),
                                    mesh.verts[0],
                                    [](auto a, auto b) { return a.cwiseMax(b).eval(); });
    
    // Generate transformation to place vertices in a unit cube
    auto scale = (maxb - minb).eval();
    scale = (scale.array().abs() > 0.00001f).select(1.f / scale, eig::Array3f(1));
    auto trf = (eig::Scaling((scale).matrix().eval()) * eig::Translation3f(-minb)).matrix().eval();

    // Apply transformation to verts
    std::for_each(std::execution::par_unseq, range_iter(mesh.verts), 
      [&](auto &v) { v = ((v - minb) * scale).eval(); });

    // Apply transformation to normals
    if (mesh.has_norms())
      std::for_each(std::execution::par_unseq, range_iter(mesh.norms), 
        [&](auto &n) { n = (n * scale).matrix().normalized().eval(); });

    return trf.inverse().eval();
  }
  
  // Adjust a mesh so there are no triangles with 0-size UVs
  template <typename MeshTy>
  void fix_degenerate_uvs(MeshTy &mesh) {
    met_trace();
    guard(mesh.has_txuvs());

    // Parallel iterate all triangle uv data 
    #pragma omp parallel for
    for (int i = 0; i < mesh.elems.size(); ++i) {
      const auto &el = mesh.elems[i];
      eig::Array2f &a = mesh.txuvs[el[0]], 
                   &b = mesh.txuvs[el[1]], 
                   &c = mesh.txuvs[el[2]];

      // If a triangle uv set is exactly identical, randomly shuffle vertex uv data
      // a tiny bit
      if (a.isApprox(b) || b.isApprox(c) || c.isApprox(a)) {
        UniformSampler sampler = i;
        #pragma omp critical
        {
          a += (sampler.next_1d() * 2.f - 1.f) * 1e-3;
          b += (sampler.next_1d() * 2.f - 1.f) * 1e-3;
          b += (sampler.next_1d() * 2.f - 1.f) * 1e-3;
        }
      }
    }
  }

  /* Explicit template instantiations */
  
#define declare_function_delaunay_output_only(OutputDelaunay)                                         \
    template                                                                                          \
    OutputDelaunay convert_mesh<OutputDelaunay>(const OutputDelaunay &);                              \
    template                                                                                          \
    OutputDelaunay generate_delaunay<OutputDelaunay, eig::Array3f>(std::span<const eig::Array3f>);    \
    template                                                                                          \
    OutputDelaunay generate_delaunay<OutputDelaunay, eig::AlArray3f>(std::span<const eig::AlArray3f>);

  #define declare_function_mesh_output_only(OutputMesh)                                               \
    template                                                                                          \
    OutputMesh convert_mesh<OutputMesh>(const OutputMesh &);                                          \
    template                                                                                          \
    void simplify_mesh<OutputMesh>(OutputMesh &, uint, float);                                        \
    template                                                                                          \
    void decimate_mesh<OutputMesh>(OutputMesh &, uint, float);                                        \
    template                                                                                          \
    void compact_mesh<OutputMesh>(OutputMesh &);                                                      \
    template                                                                                          \
    void remap_mesh<OutputMesh>(OutputMesh &);                                                        \
    template                                                                                          \
    void optimize_mesh<OutputMesh>(OutputMesh &);                                                     \
    template                                                                                          \
    void renormalize_mesh<OutputMesh>(OutputMesh &);                                                  \
    template                                                                                          \
    void fix_degenerate_uvs<OutputMesh>(OutputMesh &);                                                \
    template                                                                                          \
    std::vector<typename OutputMesh::txuv_type> parameterize_mesh<OutputMesh>(OutputMesh &);          \
    template                                                                                          \
    eig::Matrix4f unitize_mesh<OutputMesh>(OutputMesh &);                                             \
    template                                                                                          \
    OutputMesh generate_convex_hull<OutputMesh, eig::Array3f>(std::span<const eig::Array3f>);         \
    template                                                                                          \
    OutputMesh generate_convex_hull<OutputMesh, eig::AlArray3f>(std::span<const eig::AlArray3f>);

  declare_function_delaunay_output_only(Delaunay)
  declare_function_delaunay_output_only(AlDelaunay)
  declare_function_mesh_output_only(Mesh)
  declare_function_mesh_output_only(AlMesh)
} // namespace met