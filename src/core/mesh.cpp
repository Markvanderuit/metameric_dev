#include <metameric/core/mesh.hpp>
#include <metameric/core/utility.hpp>
#include <metameric/core/detail/trace.hpp>
#include <OpenMesh/Tools/Subdivider/Uniform/LoopT.hh>
#include <OpenMesh/Tools/Decimater/ModEdgeLengthT.hh>
#include <OpenMesh/Tools/Decimater/DecimaterT.hh>
#include <OpenMesh/Tools/Smoother/JacobiLaplaceSmootherT.hh>
#include <OpenMesh/Core/Utils/Property.hh>
#include <libqhullcpp/Qhull.h>
#include <libqhullcpp/QhullVertexSet.h>
#include <libqhullcpp/QhullPoints.h>
#include <fmt/ranges.h>
#include <algorithm>
#include <execution>
#include <ranges>
#include <vector>

namespace met {
  template <typename OutputMesh, typename InputMesh>
  OutputMesh convert_mesh(const InputMesh &mesh) requires std::is_same_v<OutputMesh, InputMesh> {
    met_trace_n("Passthrough");
    return mesh;
  }

  template <>
  HalfedgeMeshData convert_mesh<HalfedgeMeshData, IndexedMeshData>(const IndexedMeshData &mesh_) {
    met_trace_n("IndexedMeshData -> HalfedgeMeshData");

    const auto &[verts, elems] = mesh_;

    // Prepare mesh structure
    HalfedgeMeshData mesh;
    mesh.reserve(
      verts.size(), (verts.size() + elems.size() - 2), 
      elems.size()
    );

    // Register vertices/elements single-threaded
    std::vector<HalfedgeMeshData::VertexHandle> vth(verts.size());
    std::ranges::transform(verts, vth.begin(), [&](const auto &v) { 
      HalfedgeMeshData::Point p;
      std::ranges::copy(v, p.begin());
      return mesh.add_vertex(p);
    });
    std::ranges::for_each(elems, 
      [&](const auto &el) { return mesh.add_face(vth[el[0]], vth[el[1]], vth[el[2]]); });

    return mesh;
  }
  
  template <>
  IndexedMeshData convert_mesh<IndexedMeshData, HalfedgeMeshData>(const HalfedgeMeshData &mesh) {
    met_trace_n("HalfedgeMeshData -> IndexedMeshData");

    std::vector<eig::Array3f> verts(mesh.n_vertices());
    std::vector<eig::Array3u> faces(mesh.n_faces());

    std::transform(std::execution::par_unseq, range_iter(mesh.vertices()), verts.begin(), 
      [&](auto vh) { return convert_vector<eig::Array3f, omesh::Vec3f>(mesh.point(vh)); });
    std::transform(std::execution::par_unseq, range_iter(mesh.faces()), faces.begin(), [](auto fh) {
      eig::Array3u el; 
      std::ranges::transform(fh.vertices(), el.begin(), [](auto vh) { return vh.idx(); });
      return el;
    });
    
    return { verts, faces };
  }

  template <>
  IndexedMeshData convert_mesh<IndexedMeshData, AlignedMeshData>(const AlignedMeshData &mesh) {
    met_trace_n("AlignedMeshData -> IndexedMeshData");
    return { std::vector<eig::Array3f>(range_iter(mesh.verts)), mesh.elems };
  }

  template <>
  AlignedMeshData convert_mesh<AlignedMeshData, IndexedMeshData>(const IndexedMeshData &mesh) {
    met_trace_n("IndexedMeshData -> AlignedMeshData");
    return { std::vector<eig::AlArray3f>(range_iter(mesh.verts)), mesh.elems };
  }

  template <>
  AlignedMeshData convert_mesh<AlignedMeshData, HalfedgeMeshData>(const HalfedgeMeshData &mesh) {
    met_trace_n("HalfedgeMeshData -> AlignedMeshData");
    return convert_mesh<AlignedMeshData>(convert_mesh<IndexedMeshData>(mesh));
  }

  template <>
  HalfedgeMeshData convert_mesh<HalfedgeMeshData, AlignedMeshData>(const AlignedMeshData &mesh) {
    met_trace_n("AlignedMeshData -> HalfedgeMeshData");
    return convert_mesh<HalfedgeMeshData>(convert_mesh<IndexedMeshData>(mesh));
  }

  template <>
  IndexedMeshData convert_mesh<IndexedMeshData, IndexedDelaunayData>(const IndexedDelaunayData &mesh) {
    met_trace_n("IndexedDelaunayData -> IndexedMeshData");
    
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
  IndexedDelaunayData convert_mesh<IndexedDelaunayData, AlignedDelaunayData>(const AlignedDelaunayData &mesh) {
    met_trace_n("AlignedDelaunayData -> IndexedDelaunayData");
    return { std::vector<eig::Array3f>(range_iter(mesh.verts)), mesh.elems };
  }

  template <>
  AlignedDelaunayData convert_mesh<AlignedDelaunayData, IndexedDelaunayData>(const IndexedDelaunayData &mesh) {
    met_trace_n("IndexedDelaunayData -> AlignedDelaunayData");
    return { std::vector<eig::AlArray3f>(range_iter(mesh.verts)), mesh.elems };
  }

  template <>
  AlignedMeshData convert_mesh<AlignedMeshData, IndexedDelaunayData>(const IndexedDelaunayData &mesh) {
    met_trace_n("IndexedDelaunayData -> AlignedMeshData");
    return convert_mesh<AlignedMeshData>(convert_mesh<IndexedMeshData>(mesh));
  }

  template <>
  AlignedMeshData convert_mesh<AlignedMeshData, AlignedDelaunayData>(const AlignedDelaunayData &mesh) {
    met_trace_n("AlignedDelaunayData -> AlignedMeshData");
    return convert_mesh<AlignedMeshData>(convert_mesh<IndexedDelaunayData>(mesh));
  }

  template <typename Mesh>
  Mesh generate_octahedron() {
    met_trace();
    
    using V = eig::Array3f;
    using E = eig::Array3u;
    
    std::vector<V> verts = { V(-1.f, 0.f, 0.f ), V( 0.f,-1.f, 0.f ), V( 0.f, 0.f,-1.f ),
                             V( 1.f, 0.f, 0.f ), V( 0.f, 1.f, 0.f ), V( 0.f, 0.f, 1.f ) };
    std::vector<E> elems = { E(0, 2, 1), E(3, 1, 2), E(0, 1, 5), E(3, 5, 1),
                             E(0, 5, 4), E(3, 4, 5), E(0, 4, 2), E(3, 2, 4) };

    return convert_mesh<Mesh>(IndexedMeshData { verts, elems });
  }
  
  template <typename Mesh>
  Mesh generate_spheroid(uint n_subdivs) {
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
    
    return convert_mesh<Mesh>(mesh);
  }

  template <typename Mesh, typename Vector>
  Mesh generate_convex_hull(std::span<const Vector> data) {
    met_trace();

    // Query qhull for a convex hull structure
    std::vector<eig::Array3f> input(range_iter(data));
    auto qhull = orgQhull::Qhull("", 3, input.size(), cnt_span<const float>(input).data(), "Qt Qx C-0");
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

    return convert_mesh<Mesh>(IndexedMeshData { verts, elems });
  }
  
  template <typename Mesh, typename Vector>
  Mesh generate_delaunay(std::span<const Vector> data) {
    met_trace();

    // Query qhull for a delaunay tetrahedralization structure
    std::vector<eig::Array3f> input(range_iter(data));
    auto qhull = orgQhull::Qhull("", 3, input.size(), cnt_span<const float>(input).data(), "d Qbb Qt");
    auto qh_verts = qhull.vertexList().toStdVector();
    auto qh_elems = qhull.facetList().toStdVector();

    // Assign incremental IDs to qh_verts; Qhull does not seem to manage removed vertices properly?
    #pragma omp parallel for
    for (int i = 0; i < qh_verts.size(); ++i) 
      qh_verts[i].getVertexT()->id = i;

    // Assemble indexed data from qhull format
    std::vector<eig::Array3f> verts(qh_verts.size());
    std::vector<eig::Array4u> elems(qh_elems.size());
    std::transform(std::execution::par_unseq, range_iter(qh_verts), verts.begin(), 
      [](const auto &vt) { return eig::Array3f(vt.point().constBegin()); });

    // Undo QHull's unnecessary scatter-because-screw-you-aaaaaargh
    std::vector<uint> vertex_idx(data.size());
    for (uint i = 0; i < data.size(); ++i) {
      auto it = std::ranges::find_if(data, [&](const auto &v) { return v.isApprox(verts[i]); });
      guard_continue(it != data.end());
      vertex_idx[i] = std::distance(data.begin(), it);
    }
    
    // Build element data
    std::transform(std::execution::par_unseq, range_iter(qh_elems), elems.begin(), [&](const auto &el) {
      eig::Array4u el_;
      std::ranges::transform(el.vertices().toStdVector(), el_.begin(), 
        [&](const auto &v) { return vertex_idx[v.id()]; });
      return el_;
    });

    return convert_mesh<Mesh>(IndexedDelaunayData { std::vector<eig::Array3f>(range_iter(data)), elems });
  }

  template <typename OutputMesh, typename InputMesh>
  OutputMesh simplify_edge_length(const InputMesh &mesh_, float max_edge_length) {
    met_trace();

    namespace odec  = omesh::Decimater;
    using Module    = odec::ModEdgeLengthT<HalfedgeMeshData>::Handle;
    using Decimater = odec::DecimaterT<HalfedgeMeshData>;
    
    // Operate on a copy of the input mesh
    auto mesh = convert_mesh<HalfedgeMeshData>(mesh_);

    Decimater dec(mesh);
    Module mod;

    dec.add(mod);
    dec.module(mod).set_binary(false);
    dec.module(mod).set_edge_length(max_edge_length); // not zero, but just up to reasonable precision

    dec.initialize();
    dec.decimate();

    mesh.garbage_collection();
    return convert_mesh<OutputMesh>(mesh);
  }

  template <typename OutputMesh, typename InputMesh>
  OutputMesh simplify_volume(const InputMesh &mesh_, 
                      uint             max_vertices, 
                      const InputMesh *optional_bounds) {
    met_trace();

    namespace odec  = omesh::Decimater;
    using Module    = odec::ModVolumeT<HalfedgeMeshData>::Handle;
    using Decimater = odec::CollapsingDecimater<HalfedgeMeshData, odec::DefaultCollapseFunction>;

    // Operate on a copy of the input mesh with zero-length edges removed
    auto mesh = simplify_edge_length<HalfedgeMeshData>(mesh_, 0.f);

    Decimater dec(mesh);
    Module mod;

    dec.add(mod);

    // If provided, convert optional bounds to half edge format
    std::optional<HalfedgeMeshData> bounds_mesh;
    if (optional_bounds) {
      bounds_mesh = convert_mesh<HalfedgeMeshData>(*optional_bounds);
      dec.module(mod).set_collision_mesh(&(*bounds_mesh));
    }

    dec.initialize();
    dec.decimate_to(max_vertices);

    mesh.garbage_collection();
    return convert_mesh<OutputMesh>(mesh);
  }

  /* Explicit template instantiations */
  
#define declare_function_delaunay_output_only(OutputDelaunay)                                         \
    template                                                                                          \
    OutputDelaunay generate_delaunay<OutputDelaunay, eig::Array3f>(std::span<const eig::Array3f>);    \
    template                                                                                          \
    OutputDelaunay generate_delaunay<OutputDelaunay, eig::AlArray3f>(std::span<const eig::AlArray3f>);

  #define declare_function_mesh_output_only(OutputMesh)                                               \
    template                                                                                          \
    OutputMesh generate_octahedron<OutputMesh>();                                                     \
    template                                                                                          \
    OutputMesh generate_spheroid<OutputMesh>(uint);                                                   \
    template                                                                                          \
    OutputMesh generate_convex_hull<OutputMesh, eig::Array3f>(std::span<const eig::Array3f>);         \
    template                                                                                          \
    OutputMesh generate_convex_hull<OutputMesh, eig::AlArray3f>(std::span<const eig::AlArray3f>);

  #define declare_function_output_input(OutputMesh, InputMesh)                                        \
    template                                                                                          \
    OutputMesh simplify_edge_length<OutputMesh, InputMesh>(const InputMesh &, float);                 \
    template                                                                                          \
    OutputMesh simplify_volume<OutputMesh, InputMesh>(const InputMesh &, uint, const InputMesh *);

  #define declare_function_all_inputs(OutputMesh)                                                     \
    declare_function_mesh_output_only(OutputMesh)                                                     \
    declare_function_output_input(OutputMesh, IndexedMeshData)                                        \
    declare_function_output_input(OutputMesh, AlignedMeshData)                                        \
    declare_function_output_input(OutputMesh, HalfedgeMeshData)
  
  declare_function_delaunay_output_only(IndexedDelaunayData)
  declare_function_delaunay_output_only(AlignedDelaunayData)
  declare_function_all_inputs(IndexedMeshData)
  declare_function_all_inputs(AlignedMeshData)
  declare_function_all_inputs(HalfedgeMeshData)
} // namespace met