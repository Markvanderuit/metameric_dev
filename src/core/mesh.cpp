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
  OutputMesh optimize_mesh(const InputMesh &mesh_) {
    met_trace();
    
    return {};
  }

  template <typename OutputMesh, typename InputMesh>
  OutputMesh simplify_mesh(const InputMesh &mesh_) {
    met_trace();
    
    // Operate on a unpadded copy of the input mesh
    auto mesh = convert_mesh<Mesh>(mesh_);

    // First, remap to avoid potentially redundant vertices
    // TODO; Move to clean step
    {
      size_t n_elems = mesh.elems.size() * 3;
      size_t n_verts = mesh.verts.size();

      std::vector<eig::Array3u> remap(mesh.elems.size());

      auto dst = cnt_span<uint>(remap);
      auto src = cnt_span<uint>(mesh.elems);

      size_t vert_size = meshopt_generateVertexRemap(
        dst.data(),
        src.data(),
        src.size(),
        mesh.verts.data(),
        mesh.verts.size(),
        sizeof(Mesh::vert_type)
      );
      
      Mesh remapped_mesh;
      remapped_mesh.elems.resize(remap.size());
      remapped_mesh.verts.resize(vert_size);
      remapped_mesh.norms.resize(vert_size);
      remapped_mesh.txuvs.resize(vert_size);
      meshopt_remapIndexBuffer(cnt_span<uint>(remapped_mesh.elems).data(), src.data(), src.size(), dst.data());
      meshopt_remapVertexBuffer(remapped_mesh.verts.data(), mesh.verts.data(), mesh.verts.size(), sizeof(Mesh::vert_type), dst.data());
      meshopt_remapVertexBuffer(remapped_mesh.norms.data(), mesh.norms.data(), mesh.norms.size(), sizeof(Mesh::norm_type), dst.data());
      meshopt_remapVertexBuffer(remapped_mesh.txuvs.data(), mesh.txuvs.data(), mesh.txuvs.size(), sizeof(Mesh::txuv_type), dst.data());
      mesh = remapped_mesh;
    }

    float threshold    = 0.05f;
    float target_error = 1.f; // 1e-1f;
    float lod_error    = 1.f;
    uint options       = 0; // meshopt_SimplifyX flags, 0 is a safe default

    size_t n_elems = mesh.elems.size() * 3;
    size_t n_elems_target = static_cast<size_t>(n_elems * threshold);
    fmt::print("Target = {}\n", n_elems_target);
    
    // Run meshoptimizer's simplify
    std::vector<uint> target(n_elems);
    target.resize(meshopt_simplify(target.data(), 
                                   &mesh.elems[0][0], 
                                   n_elems, 
                                   &mesh.verts[0][0],
                                   mesh.verts.size(),
                                   sizeof(Mesh::vert_type),
                                   n_elems_target,
                                   target_error,
                                   options,
                                  &lod_error));
    fmt::print("Reached = {}\n", target.size());

    // Copy over elements
    // NOTE; this does not account for deleted vertices!!!
    mesh.elems.resize(target.size() / 3);
    rng::copy(cnt_span<Mesh::elem_type>(target), mesh.elems.begin());
    
    // Return correct expected type
    return convert_mesh<OutputMesh>(mesh);
  }

  // template <typename OutputMesh, typename InputMesh>
  // OutputMesh simplify_edge_length(const InputMesh &mesh_, float max_edge_length) {
  //   met_trace();

  //   namespace odec  = omesh::Decimater;
  //   using Module    = odec::ModEdgeLengthT<HalfedgeMeshData>::Handle;
  //   using Decimater = odec::DecimaterT<HalfedgeMeshData>;
    
  //   // Operate on a copy of the input mesh
  //   auto mesh = convert_mesh<HalfedgeMeshData>(mesh_);

  //   Decimater dec(mesh);
  //   Module mod;

  //   dec.add(mod);
  //   dec.module(mod).set_binary(false);
  //   dec.module(mod).set_edge_length(max_edge_length); // not zero, but just up to reasonable precision

  //   dec.initialize();
  //   dec.decimate();

  //   mesh.garbage_collection();
  //   return convert_mesh<OutputMesh>(mesh);
  // }
  
  // template <typename OutputMesh, typename InputMesh>
  // OutputMesh simplify_progressive(const InputMesh &mesh_, uint max_vertices) {
  //   met_trace();

  //   namespace odec  = omesh::Decimater;
  //   // using Module    = odec::ModEdgeLengthT<HalfedgeMeshData>::Handle;
  //   // using Decimater = odec::DecimaterT<HalfedgeMeshData>;
  //   using Module    = odec::ModEdgeLengthT<HalfedgeMeshData>::Handle;
  //   using Decimater = odec::CollapsingDecimater<HalfedgeMeshData, odec::AverageCollapseFunction>;
  //   // using Module    = odec::ModQuadricT<HalfedgeMeshData>::Handle;
  //   // using Decimater = odec::CollapsingDecimater<HalfedgeMeshData, odec::DefaultCollapseFunction>;
  //   // using ModuleN   = odec::ModNormalFlippingT<HalfedgeMeshData>::Handle;

  //   // Operate on a copy of the input mesh
  //   HalfedgeMeshData mesh = convert_mesh<HalfedgeMeshData>(mesh_);
    
  //   // Face normals are not valid at this point
  //   mesh.request_face_normals();
  //   mesh.update_face_normals();

  //   Decimater dec(mesh);
  //   Module mod;
  //   dec.add(mod);

  //   dec.module(mod).set_binary(false);
  //   dec.module(mod).set_edge_length(std::numeric_limits<float>::max());
  //   // ModuleN modn;
  //   // dec.add(modn);
  //   // dec.module(mod).unset_max_err();

  //   // dec.module(mod).set_binary(false);
  //   // dec.module(mod).set_edge_length(std::numeric_limits<float>::max());
  //   // dec.initialize();
  //   // dec.decimate(1000);
  //   // // dec.decimate_to(max_vertices);
  //   // mesh.garbage_collection();

  //   dec.initialize();
  //   dec.decimate_to(max_vertices);
  //   mesh.garbage_collection();

  //   return convert_mesh<OutputMesh>(mesh);
  // }

  // template <typename OutputMesh, typename InputMesh>
  // OutputMesh simplify_volume(const InputMesh &mesh_, 
  //                     uint             max_vertices, 
  //                     const InputMesh *optional_bounds) {
  //   met_trace();

  //   namespace odec  = omesh::Decimater;
  //   using Module    = odec::ModVolumeT<HalfedgeMeshData>::Handle;
  //   using Decimater = odec::CollapsingDecimater<HalfedgeMeshData, odec::DefaultCollapseFunction>;

  //   // Operate on a copy of the input mesh with zero-length edges removed
  //   auto mesh = simplify_edge_length<HalfedgeMeshData>(mesh_, 0.f);

  //   Decimater dec(mesh);
  //   Module mod;

  //   dec.add(mod);

  //   // If provided, convert optional bounds to half edge format
  //   std::optional<HalfedgeMeshData> bounds_mesh;
  //   if (optional_bounds) {
  //     bounds_mesh = convert_mesh<HalfedgeMeshData>(*optional_bounds);
  //     dec.module(mod).set_collision_mesh(&(*bounds_mesh));
  //   }

  //   dec.initialize();
  //   dec.decimate_to(max_vertices);
  //   mesh.garbage_collection();

  //   return convert_mesh<OutputMesh>(mesh);
  // }

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
    OutputMesh simplify_mesh<OutputMesh, InputMesh>(const InputMesh &);                               \
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