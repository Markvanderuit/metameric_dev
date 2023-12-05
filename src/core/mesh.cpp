#include <metameric/core/mesh.hpp>
#include <metameric/core/utility.hpp>
#include <metameric/core/detail/trace.hpp>
#include <meshoptimizer.h>
#include <libqhullcpp/Qhull.h>
#include <libqhullcpp/QhullVertexSet.h>
#include <libqhullcpp/QhullPoints.h>
#include <fmt/ranges.h>
#include <algorithm>
#include <execution>
#include <ranges>
#include <vector>

namespace met {
  // template <>
  // HalfedgeMeshData convert_mesh<HalfedgeMeshData, Mesh>(const Mesh &mesh_) {
  //   met_trace_n("Mesh -> HalfedgeMeshData");

  //   // UV and normal data is lost during conversion
  //   const auto &[verts, elems, norms, txuvs] = mesh_;

  //   // Prepare mesh structure
  //   HalfedgeMeshData mesh;
  //   mesh.reserve(verts.size(), (verts.size() + elems.size() - 2), elems.size());

  //   // Register vertices/elements single-threaded
  //   std::vector<HalfedgeMeshData::VertexHandle> vth(verts.size());
  //   for (uint i = 0; i < vth.size(); ++i) {
  //     HalfedgeMeshData::Point m_vt;
  //     rng::copy(verts[i], m_vt.begin());
  //     auto vh = mesh.add_vertex(m_vt);

  //     if (!norms.empty()) {
  //       HalfedgeMeshData::Normal m_nr;
  //       rng::copy(norms[i], m_nr.begin());
  //       mesh.set_normal(vh, m_nr);
  //     }      

  //     if (!txuvs.empty()) {
  //       HalfedgeMeshData::TexCoord2D m_tx;
  //       rng::copy(txuvs[i], m_tx.begin());
  //       mesh.set_texcoord2D(vh, m_tx);
  //     }

  //     vth[i] = std::move(vh);
  //   } // for (uint i)

  //   // Register faces
  //   rng::for_each(elems, 
  //     [&](const auto &el) { return mesh.add_face(vth[el[0]], vth[el[1]], vth[el[2]]); });

  //   return mesh;
  // }
  
  // template <>
  // Mesh convert_mesh<Mesh, HalfedgeMeshData>(const HalfedgeMeshData &mesh) {
  //   met_trace_n("HalfedgeMeshData -> Mesh");
    
  //   Mesh cmesh;
  //   cmesh.verts.resize(mesh.n_vertices());
  //   cmesh.elems.resize(mesh.n_faces());

  //   // Generate vertex data
  //   std::transform(std::execution::par_unseq, range_iter(mesh.vertices()), cmesh.verts.begin(), 
  //     [&](auto vh) { return convert_vector<eig::Array3f, omesh::Vec3f>(mesh.point(vh)); });


  //   // Generate optional data
  //   if (mesh.has_vertex_normals()) {
  //     cmesh.norms.resize(mesh.n_vertices());
  //     std::transform(std::execution::par_unseq, range_iter(mesh.vertices()), cmesh.norms.begin(), 
  //       [&](auto vh) { return convert_vector<eig::Array3f, omesh::Vec3f>(mesh.normal(vh)); });
  //   }
  //   if (mesh.has_vertex_texcoords2D()) {
  //     cmesh.txuvs.resize(mesh.n_vertices());
  //     std::transform(std::execution::par_unseq, range_iter(mesh.vertices()), cmesh.txuvs.begin(), 
  //       [&](auto vh) { return convert_vector<eig::Array2f, omesh::Vec2f>(mesh.texcoord2D(vh)); });
  //   }

  //   // Generate element data
  //   std::transform(std::execution::par_unseq, range_iter(mesh.faces()), cmesh.elems.begin(), 
  //   [](auto fh) {
  //     eig::Array3u el; 
  //     rng::transform(fh.vertices(), el.begin(), [](auto vh) { return vh.idx(); });
  //     return el;
  //   });
    
  //   return cmesh;
  // }

  template <typename OutputMesh, typename InputMesh>
  OutputMesh convert_mesh(const InputMesh &mesh) requires std::is_same_v<OutputMesh, InputMesh> {
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

  // template <>
  // AlMesh convert_mesh<AlMesh, HalfedgeMeshData>(const HalfedgeMeshData &mesh) {
  //   met_trace_n("HalfedgeMeshData -> AlMeshData");
  //   return convert_mesh<AlMesh>(convert_mesh<Mesh>(mesh));
  // }

  // template <>
  // HalfedgeMeshData convert_mesh<HalfedgeMeshData, AlMesh>(const AlMesh &mesh) {
  //   met_trace_n("AlMeshData -> HalfedgeMeshData");
  //   return convert_mesh<HalfedgeMeshData>(convert_mesh<Mesh>(mesh));
  // }

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

 /*  template <typename MeshTy>
  MeshTy generate_octahedron() {
    met_trace();
    
    using V = eig::Array3f;
    using E = eig::Array3u;
    
    std::vector<V> verts = { V(-1.f, 0.f, 0.f ), V( 0.f,-1.f, 0.f ), V( 0.f, 0.f,-1.f ),
                             V( 1.f, 0.f, 0.f ), V( 0.f, 1.f, 0.f ), V( 0.f, 0.f, 1.f ) };
    std::vector<E> elems = { E(0, 2, 1), E(3, 1, 2), E(0, 1, 5), E(3, 5, 1),
                             E(0, 5, 4), E(3, 4, 5), E(0, 4, 2), E(3, 2, 4) };

    return convert_mesh<MeshTy>(Mesh { verts, elems });
  } */
  
  /* template <typename MeshTy>
  MeshTy generate_spheroid(uint n_subdivs) {
    met_trace();

    // Start with an octahedron; doing loop subdivision and normalizing the resulting vertices
    // naturally leads to a spheroid with unit vectors as its vertices
    auto mesh = generate_octahedron<HalfedgeMeshData>();
    
    // Construct loop subdivider
    omesh::Subdivider::Uniform::LoopT<
      decltype(mesh), 
      decltype(mesh)::Point::value_type
    > subdivider;

    // Perform subdivision n times
    subdivider.attach(mesh);
    subdivider(n_subdivs);
    subdivider.detach();

    // Normalize each resulting vertex point
    std::for_each(std::execution::par_unseq, range_iter(mesh.vertices()),
      [&](auto vh) { mesh.point(vh).normalize(); });
    
    return convert_mesh<MeshTy>(mesh);
  } */

  template <typename MeshTy, typename Vector>
  MeshTy generate_convex_hull(std::span<const Vector> data) {
    met_trace();

    // Query qhull for a convex hull structure
    std::vector<eig::Array3f> input(range_iter(data));
    auto qhull = orgQhull::Qhull(
      "", 3, input.size(), cnt_span<const float>(input).data(), "Qt Qx C-0"
      // "", 3, input.size(), cnt_span<const float>(input).data(), "QJ Q0 Po Pp"
    );
      
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
      [](const auto &vt) { return eig::Array3f(vt.point().constBegin()); });

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

    std::vector<eig::Array3f> input(range_iter(data));

    std::vector<orgQhull::QhullVertex> qh_verts;
    std::vector<orgQhull::QhullFacet>  qh_elems;
    { // qhull
      met_trace_n("qhull_generate_delaunay");
      auto qhull = orgQhull::Qhull("", 3, input.size(), cnt_span<const float>(input).data(), "d Qbb Qt");
      qh_verts = qhull.vertexList().toStdVector();
      qh_elems = qhull.facetList().toStdVector();
    } // qhull

    // Assign incremental IDs to qh_verts; Qhull does not seem to manage removed vertices properly?
    #pragma omp parallel for
    for (int i = 0; i < qh_verts.size(); ++i) 
      qh_verts[i].getVertexT()->id = i;

    // Assemble indexed data from qhull format
    std::vector<eig::Array3f> verts(qh_verts.size());
    std::transform(std::execution::par_unseq, range_iter(qh_verts), verts.begin(), 
      [](const auto &vt) { return Mesh::vert_type(vt.point().constBegin()); });

    // Undo QHull's unnecessary scatter-because-screw-you-aaaaaargh
    std::vector<uint> vertex_idx(data.size());
    #pragma omp parallel for
    for (int i = 0; i < vertex_idx.size(); ++i) {
      auto it = rng::find_if(data, [&](const auto &v) { return v.isApprox(verts[i]); });
      guard_continue(it != data.end());
      vertex_idx[i] = std::distance(data.begin(), it);
    }
    
    // Build element data
    std::vector<eig::Array4u> elems(qh_elems.size());
    std::transform(std::execution::par_unseq, range_iter(qh_elems), elems.begin(), [&](const auto &el) {
      eig::Array4u el_;
      std::ranges::transform(el.vertices().toStdVector(), el_.begin(), 
        [&](const auto &v) { return vertex_idx[v.id()]; });
      return el_;
    });

    return convert_mesh<MeshTy>(Delaunay { std::vector<eig::Array3f>(range_iter(data)), elems });
  }

  template <typename OutputMesh, typename InputMesh>
  OutputMesh remap_mesh(const InputMesh &mesh) {
    met_trace();

    // First, add all mesh attribute streams that need remapping
    std::vector<meshopt_Stream> attribute_streams;
    attribute_streams.push_back({ mesh.verts.data(), sizeof(eig::Array3f), sizeof(InputMesh::vert_type) });
    if (!mesh.norms.empty())
      attribute_streams.push_back({ mesh.norms.data(), sizeof(eig::Array3f), sizeof(InputMesh::norm_type) });
    if (!mesh.txuvs.empty())
      attribute_streams.push_back({ mesh.txuvs.data(), sizeof(eig::Array2f), sizeof(InputMesh::txuv_type) });

    // Second, get spans over source/destination index memory, then generate remapping
    std::vector<eig::Array3u> remap(mesh.verts.size());
    auto dst = cnt_span<uint>(remap);
    auto src = cnt_span<const uint>(mesh.elems);
    size_t n_remapped_verts = meshopt_generateVertexRemapMulti(
      dst.data(), 
      src.data(),
      src.size(),
      mesh.verts.size(),
      attribute_streams.data(),
      attribute_streams.size()
    );

    // Third, create new mesh to remap into
    InputMesh mesh_;
    mesh_.elems.resize(mesh.elems.size());
    mesh_.verts.resize(n_remapped_verts);
    if (!mesh.norms.empty())
      mesh_.norms.resize(n_remapped_verts);
    if (!mesh.txuvs.empty())
      mesh_.txuvs.resize(n_remapped_verts);

    // Let meshopt do remapping
    meshopt_remapIndexBuffer(cnt_span<uint>(mesh_.elems).data(), src.data(), src.size(), dst.data());
    meshopt_remapVertexBuffer(mesh_.verts.data(), mesh.verts.data(), mesh.verts.size(), sizeof(InputMesh::vert_type), dst.data());
    if (!mesh.norms.empty())
      meshopt_remapVertexBuffer(mesh_.norms.data(), mesh.norms.data(), mesh.norms.size(), sizeof(InputMesh::norm_type), dst.data());
    if (!mesh.txuvs.empty())
      meshopt_remapVertexBuffer(mesh_.txuvs.data(), mesh.txuvs.data(), mesh.txuvs.size(), sizeof(InputMesh::txuv_type), dst.data());

    return convert_mesh<OutputMesh>(mesh_);
  }

  template <typename OutputMesh, typename InputMesh>
  OutputMesh compact_mesh(const InputMesh &mesh) {
    met_trace();

    // First, get spans over source/destination index memory, then generate remapping
    std::vector<eig::Array3u> remap(mesh.verts.size());
    auto dst = cnt_span<uint>(remap);
    auto src = cnt_span<const uint>(mesh.elems);
    size_t n_remapped_verts = meshopt_optimizeVertexFetchRemap(
      dst.data(),
      src.data(),
      src.size(),
      mesh.verts.size()
    );

    // Second, create new mesh to remap into
    InputMesh mesh_;
    mesh_.elems.resize(mesh.elems.size());
    mesh_.verts.resize(n_remapped_verts);
    if (!mesh.norms.empty())
      mesh_.norms.resize(n_remapped_verts);
    if (!mesh.txuvs.empty())
      mesh_.txuvs.resize(n_remapped_verts);

    // Let meshopt do remapping
    meshopt_remapIndexBuffer(cnt_span<uint>(mesh_.elems).data(), src.data(), src.size(), dst.data());
    meshopt_remapVertexBuffer(mesh_.verts.data(), mesh.verts.data(), mesh.verts.size(), sizeof(InputMesh::vert_type), dst.data());
    if (!mesh.norms.empty())
      meshopt_remapVertexBuffer(mesh_.norms.data(), mesh.norms.data(), mesh.norms.size(), sizeof(InputMesh::norm_type), dst.data());
    if (!mesh.txuvs.empty())
      meshopt_remapVertexBuffer(mesh_.txuvs.data(), mesh.txuvs.data(), mesh.txuvs.size(), sizeof(InputMesh::txuv_type), dst.data());
    
    return convert_mesh<OutputMesh>(mesh_);
  }

  
  // (Re)compute vertex normals from scratch
  template <typename OutputMesh, typename InputMesh>
  OutputMesh renormalize_mesh(const InputMesh &mesh_) {
    met_trace();

    // Prepare output mesh
    auto mesh = convert_mesh<OutputMesh>(mesh_);
    mesh.norms = std::vector<typename OutputMesh::norm_type>(mesh.verts.size(), OutputMesh::norm_type(0));

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

    return mesh;
  }

  template <typename OutputMesh, typename InputMesh>
  OutputMesh simplify_mesh(const InputMesh &mesh_, uint target_elems, float target_error) {
    met_trace();
    
    // Operate on a unpadded and correctly indexed copy of the input mesh
    auto mesh = remap_mesh<Mesh>(mesh_);
    
    // Generate output/input ranges
    std::vector<eig::Array3u> remap(mesh.elems.size());
    auto dst = cnt_span<uint>(remap);
    auto src = cnt_span<uint>(mesh.elems);
    
    // Run meshoptimizer's simplify
    size_t elem_size = meshopt_simplify(dst.data(), 
                                        src.data(), 
                                        src.size(), 
                                        mesh.verts[0].data(),
                                        mesh.verts.size(),
                                        sizeof(Mesh::vert_type),
                                        target_elems * 3,
                                        target_error,
                                        0,
                                        nullptr);

    // Copy over elements
    remap.resize(elem_size / 3);
    remap.shrink_to_fit();
    mesh.elems = remap;

    // Return correct expected type, discarding unused vertices
    return compact_mesh<OutputMesh>(mesh);
  }

  template <typename OutputMesh, typename InputMesh>
  OutputMesh decimate_mesh(const InputMesh &mesh_, uint target_elems, float target_error) {
    met_trace();
    
    // Operate on a unpadded and correctly indexed copy of the input mesh
    auto mesh = remap_mesh<Mesh>(mesh_);
    
    // Generate output/input ranges
    std::vector<eig::Array3u> remap(mesh.elems.size());
    auto dst = cnt_span<uint>(remap);
    auto src = cnt_span<const uint>(mesh.elems);
    
    // Run meshoptimizer's simplifySloppy
    size_t elem_size = meshopt_simplifySloppy(dst.data(), 
                                              src.data(), 
                                              src.size(), 
                                              mesh.verts[0].data(),
                                              mesh.verts.size(),
                                              sizeof(Mesh::vert_type),
                                              target_elems * 3,
                                              target_error,
                                              nullptr);

    // Copy over elements
    remap.resize(elem_size / 3);
    remap.shrink_to_fit();
    mesh.elems = remap;

    // Return correct expected type, discarding unused vertices
    return compact_mesh<OutputMesh>(mesh);
  }

  template <typename OutputMesh, typename InputMesh>
  OutputMesh optimize_mesh(const InputMesh &mesh_) {
    met_trace();
    // ...
    return {};
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
    OutputMesh generate_convex_hull<OutputMesh, eig::Array3f>(std::span<const eig::Array3f>);         \
    template                                                                                          \
    OutputMesh generate_convex_hull<OutputMesh, eig::AlArray3f>(std::span<const eig::AlArray3f>);

  #define declare_function_output_input(OutputMesh, InputMesh)                                        \
    template                                                                                          \
    OutputMesh remap_mesh<OutputMesh, InputMesh>(const InputMesh &);                                  \
    template                                                                                          \
    OutputMesh renormalize_mesh<OutputMesh, InputMesh>(const InputMesh &);                            \
    template                                                                                          \
    OutputMesh compact_mesh<OutputMesh, InputMesh>(const InputMesh &);                                \
    template                                                                                          \
    OutputMesh simplify_mesh<OutputMesh, InputMesh>(const InputMesh &, uint, float);                  \
    template                                                                                          \
    OutputMesh decimate_mesh<OutputMesh, InputMesh>(const InputMesh &, uint, float);                  \
    template                                                                                          \
    OutputMesh optimize_mesh<OutputMesh, InputMesh>(const InputMesh &);

  #define declare_function_all_inputs(OutputMesh)                                                     \
    declare_function_mesh_output_only(OutputMesh)                                                     \
    declare_function_output_input(OutputMesh, Mesh)                                                   \
    declare_function_output_input(OutputMesh, AlMesh)
  
  declare_function_delaunay_output_only(Delaunay)
  declare_function_delaunay_output_only(AlDelaunay)
  declare_function_all_inputs(Mesh)
  declare_function_all_inputs(AlMesh)
} // namespace met