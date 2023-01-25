#include <metameric/core/mesh.hpp>
#include <metameric/core/utility.hpp>
#include <metameric/core/detail/trace.hpp>
#include <OpenMesh/Tools/Subdivider/Uniform/LoopT.hh>
#include <OpenMesh/Tools/Decimater/ModQuadricT.hh>
#include <OpenMesh/Tools/Decimater/ModNormalDeviationT.hh>
#include <OpenMesh/Tools/Decimater/ModNormalFlippingT.hh>
#include <OpenMesh/Tools/Decimater/ModEdgeLengthT.hh>
#include <OpenMesh/Tools/Decimater/ModAspectRatioT.hh>
#include <OpenMesh/Tools/Decimater/DecimaterT.hh>
#include <OpenMesh/Tools/Decimater/MixedDecimaterT.hh>
#include <OpenMesh/Tools/Smoother/JacobiLaplaceSmootherT.hh>
#include <OpenMesh/Core/Utils/Property.hh>
#include <quickhull/QuickHull.hpp>
#include <array>
#include <algorithm>
#include <execution>
#include <ranges>
#include <numbers>
#include <unordered_set>
#include <unordered_map>
#include <vector>

namespace met {
  namespace detail {
    template <typename T>
    constexpr
    auto eig_hash = [](const auto &mat) {
      size_t seed = 0;
      for (size_t i = 0; i < mat.size(); ++i) {
        auto elem = *(mat.data() + i);
        seed ^= std::hash<T>()(elem) + 0x9e3779b9 + (seed << 6) + (seed >> 2);
      }
      return seed;
    };

    // key_equal for eigen types for std::unordered_map/unordered_set
    constexpr 
    auto eig_equal = [](const auto &a, const auto &b) { 
      return a.isApprox(b); 
    };

    constexpr
    auto eig_add = [](const auto &a, const auto &b) { return (a + b).eval(); };

    template <typename T>
    constexpr
    auto omesh_hash = [](const auto &v) {
      size_t seed = 0;
      for (size_t i = 0; i < 3; ++i) {
        auto elem = v[i];
        seed ^= std::hash<T>()(v[i]) + 0x9e3779b9 + (seed << 6) + (seed >> 2);
      }
      return seed;
    };

    template <typename T>
    constexpr
    auto vert_hash = [](const auto &vert) {
      return std::hash<T>(vert.idx());
    };
  } // namespace detail

  template <typename Traits, typename T>
  std::pair<std::vector<T>, std::vector<eig::Array3u>> generate_data(const TriMesh<Traits> &mesh) {
    met_trace();

    std::vector<T> vertices(mesh.n_vertices());
    std::vector<eig::Array3u> elements(mesh.n_faces());

    std::transform(std::execution::par_unseq, range_iter(mesh.vertices()), vertices.begin(), [&](auto vh) { 
      return to_eig<float, 3>(mesh.point(vh));
    });
    std::transform(std::execution::par_unseq, range_iter(mesh.faces()), elements.begin(), [](auto fh) {
      eig::Array3u el; 
      std::ranges::transform(fh.vertices(), el.begin(), [](auto vh) { return vh.idx(); });
      return el;
    });

    return { vertices, elements };
  }

  template <typename Traits, typename T>
  TriMesh<Traits> generate_from_data(std::span<const T> vertices, std::span<const eig::Array3u> elements) {
    met_trace();

    TriMesh<Traits> mesh;
    mesh.reserve(vertices.size(), (vertices.size() + elements.size() - 2), elements.size());

    std::vector<typename decltype(mesh)::VertexHandle> vth(vertices.size());
    std::ranges::transform(vertices, vth.begin(), [&](const auto &v) { 
      typename decltype(mesh)::Point p;
      std::ranges::copy(v, p.begin());
      return mesh.add_vertex(p);
    });
    std::ranges::for_each(elements, [&](const auto &e) { 
      return mesh.add_face(vth[e[0]], vth[e[1]], vth[e[2]]); 
    });

    return mesh;
  }

  template <typename Traits>
  TriMesh<Traits> generate_octahedron() {
    met_trace();

    using V = eig::Array3f;
    using E = eig::Array3u;

    std::array<V, 6> verts = { V(-1.f, 0.f, 0.f ), V( 0.f,-1.f, 0.f ), V( 0.f, 0.f,-1.f ),
                               V( 1.f, 0.f, 0.f ), V( 0.f, 1.f, 0.f ), V( 0.f, 0.f, 1.f ) };
    std::array<E, 8> elems = { E(0, 2, 1), E(3, 1, 2), E(0, 1, 5), E(3, 5, 1),
                               E(0, 5, 4), E(3, 4, 5), E(0, 4, 2), E(3, 2, 4) };

    return generate_from_data<Traits>(std::span<const V> { verts }, std::span<const E> { elems });
  }

  template <typename Traits>
  TriMesh<Traits> generate_spheroid(uint n_subdivs) {
    met_trace();

    // Start with an octahedron; doing loop subdivision and normalizing the resulting vertices
    // naturally leads to a spheroid with unit vectors as its vertices
    auto mesh = generate_octahedron<Traits>();
    
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
      
    return mesh;
  }

  template <typename Traits, typename T>
  TriMesh<Traits> generate_convex_hull(std::span<const T> points) {
    met_trace();

    using namespace quickhull;

    using Vector3f = quickhull::Vector3<float>;

    std::vector<Vector3f> input(points.size());
    std::ranges::transform(points, input.begin(), [](const T &c) { return Vector3f { c[0], c[1], c[2] }; });

    QuickHull<float> builder;
    auto chull = builder.getConvexHull(input.data(), input.size(), false, false);

    const auto& elems = chull.getIndexBuffer();
    const auto& verts = chull.getVertexBuffer();

    std::vector<T> output_verts(verts.size());
    std::ranges::transform(verts, output_verts.begin(),  [](const Vector3f &c) { return T { c.x, c.y, c.z }; });

    std::vector<eig::Array3u> output_elems(elems.size() / 3);
    for (uint i = 0; i < elems.size() / 3; ++i)
      output_elems[i] = { static_cast<uint>(elems[3 * i]),  static_cast<uint>(elems[3 * i + 1]), static_cast<uint>(elems[3 * i + 2]) };
    
    return generate_from_data<Traits>(std::span<const T> { output_verts },
                                      std::span<const eig::Array3u> { output_elems });
  }
  
  template <typename T>
  std::pair<std::vector<T>, std::vector<eig::Array3u>> generate_convex_hull(std::span<const T> points) {
    met_trace();

    using namespace quickhull;

    using Vector3f = quickhull::Vector3<float>;

    std::vector<Vector3f> input(points.size());
    std::ranges::transform(points, input.begin(), [](const T &c) { return Vector3f { c[0], c[1], c[2] }; });

    QuickHull<float> builder;
    auto chull = builder.getConvexHull(input.data(), input.size(), false, false);

    const auto& elems = chull.getIndexBuffer();
    const auto& verts = chull.getVertexBuffer();

    std::vector<T> output_verts(verts.size());
    std::ranges::transform(verts, output_verts.begin(),  [](const Vector3f &c) { return T { c.x, c.y, c.z }; });

    std::vector<eig::Array3u> output_elems(elems.size() / 3);
    for (uint i = 0; i < elems.size() / 3; ++i)
      output_elems[i] = { static_cast<uint>(elems[3 * i]),  static_cast<uint>(elems[3 * i + 1]), static_cast<uint>(elems[3 * i + 2]) };

    return { output_verts, output_elems };
  }

  template <typename Traits, typename T>
  TriMesh<Traits> generate_convex_hull_approx(std::span<const T> points, const TriMesh<Traits> &spheroid_mesh) {
    met_trace();

    auto mesh = spheroid_mesh;

    // Compute centroid of input points
    T cntr = std::reduce(std::execution::par_unseq, range_iter(points), T(0.f), detail::eig_add)
           / static_cast<float>(points.size());

    // For each vertex in mesh, each defining a unit direction and therefore line through the origin:
    std::for_each(std::execution::par_unseq, range_iter(mesh.vertices()), [&](auto &vh) {
      met_trace();

      auto v = to_eig(mesh.point(vh));

      // Obtain a range of point projections along this line
      auto proj_funct = [&](const auto &p) { return v.matrix().dot((p - cntr).matrix());  };
      auto proj_range = points | std::views::transform(proj_funct);

      // Provide iterator to endpoint, given the largest point projection
      auto proj_maxel = std::ranges::max_element(proj_range);
      
      // Replace mesh vertex with this endpoint
      mesh.point(vh) = to_omesh<float, 3>(points[std::distance(proj_range.begin(), proj_maxel)].matrix());
    });

    return mesh;
  }

  
  template <typename Traits>
  TriMesh<Traits> simplify_edges(const TriMesh<Traits> &input_mesh, 
                                 float max_edge_length) {
    met_trace();

    namespace odec  = omesh::Decimater;
    using Mesh      = TriMesh<Traits>;
    using Module    = odec::ModEdgeLengthT<Mesh>::Handle;
    using Decimater = odec::DecimaterT<Mesh>;

    Mesh mesh = input_mesh;

    Decimater dec(mesh);
    Module mod_edge_len;

    dec.add(mod_edge_len);
    dec.module(mod_edge_len).set_binary(false);
    dec.module(mod_edge_len).set_edge_length(0.005f); // not zero, but just up to reasonable precision

    dec.initialize();
    dec.decimate();

    mesh.garbage_collection();
    
    return mesh;
  }

  template <typename Traits>
  TriMesh<Traits> simplify(const TriMesh<Traits> &input_mesh, 
                           const TriMesh<Traits> &bounds_mesh,
                           uint max_vertices) {
    met_trace();
    namespace odec = omesh::Decimater;
    using Mesh = TriMesh<Traits>;
    using Prop = omesh::HPropHandleT<omesh::Vec3f>;

    // Operate on a copy of the input mesh
    Mesh mesh = input_mesh;
    size_t pre_vertex_count, post_vertex_count;
    
    // First, quickly collapse all very short edges into their average to a hardcoded minimum;
    // given that convex hull generation is relatively accurate, this likely does not affect anything
    pre_vertex_count = mesh.n_vertices();
    {
      using ModEdgeLen = odec::ModEdgeLengthT<Mesh>::Handle;
      using Decimater  = odec::DecimaterT<Mesh>;

      Decimater dec(mesh);
      ModEdgeLen mod_edge_len;

      dec.add(mod_edge_len);
      dec.module(mod_edge_len).set_binary(false);
      dec.module(mod_edge_len).set_edge_length(0.0f); // not zero, but just up to reasonable precision

      dec.initialize();
      dec.decimate();

      mesh.garbage_collection();
    }
    post_vertex_count = mesh.n_vertices();
    fmt::print("  zero-edge collapse; {} -> {}", pre_vertex_count, post_vertex_count);
    
    // Next, collapse remaining edges using more complicated metric to get to specified vertex amount
    pre_vertex_count = mesh.n_vertices();
    {
      using Decimater  = odec::CollapsingDecimater<Mesh, odec::DefaultCollapseFunction>;
      using ModVolume = odec::ModVolumeT<Mesh>::Handle;

      Decimater dec(mesh);
      ModVolume volume_mod;

      dec.add(volume_mod);
      dec.module(volume_mod).set_collision_mesh(&bounds_mesh);

      dec.initialize();
      dec.decimate_to(max_vertices);

      mesh.garbage_collection();
    }
    post_vertex_count = mesh.n_vertices();
    fmt::print("  volume preserving collapse; {} -> {}", pre_vertex_count, post_vertex_count);

    return mesh;
  }

  /* Forward declarations over common OpenMesh types and Array3f/AlArray3f */
  
  // generate_data
  template
  std::pair<std::vector<eig::Array3f>, std::vector<eig::Array3u>> generate_data<BaselineMeshTraits, eig::Array3f>(const BaselineMesh &);
  template
  std::pair<std::vector<eig::Array3f>, std::vector<eig::Array3u>> generate_data<FNormalMeshTraits, eig::Array3f>(const FNormalMesh &);
  template
  std::pair<std::vector<eig::Array3f>, std::vector<eig::Array3u>> generate_data<HalfedgeMeshTraits, eig::Array3f>(const HalfedgeMesh &);
  template
  std::pair<std::vector<eig::AlArray3f>, std::vector<eig::Array3u>> generate_data<BaselineMeshTraits, eig::AlArray3f>(const BaselineMesh &);
  template
  std::pair<std::vector<eig::AlArray3f>, std::vector<eig::Array3u>> generate_data<FNormalMeshTraits, eig::AlArray3f>(const FNormalMesh &);
  template
  std::pair<std::vector<eig::AlArray3f>, std::vector<eig::Array3u>> generate_data<HalfedgeMeshTraits, eig::AlArray3f>(const HalfedgeMesh &);

  // generate_from_data
  template
  BaselineMesh generate_from_data<BaselineMeshTraits, eig::Array3f>(std::span<const eig::Array3f>, std::span<const eig::Array3u>);
  template
  BaselineMesh generate_from_data<BaselineMeshTraits, eig::Array3f>(std::span<const eig::Array3f>, std::span<const eig::Array3u>);
  template
  FNormalMesh generate_from_data<FNormalMeshTraits, eig::Array3f>(std::span<const eig::Array3f>, std::span<const eig::Array3u>);
  template
  FNormalMesh generate_from_data<FNormalMeshTraits, eig::AlArray3f>(std::span<const eig::AlArray3f>, std::span<const eig::Array3u>);
  template
  HalfedgeMesh generate_from_data<HalfedgeMeshTraits, eig::AlArray3f>(std::span<const eig::AlArray3f>, std::span<const eig::Array3u>);
  template
  HalfedgeMesh generate_from_data<HalfedgeMeshTraits, eig::AlArray3f>(std::span<const eig::AlArray3f>, std::span<const eig::Array3u>);

  // generate_octahedron
  template
  BaselineMesh generate_octahedron<BaselineMeshTraits>();
  template
  FNormalMesh generate_octahedron<FNormalMeshTraits>();
  template
  HalfedgeMesh generate_octahedron<HalfedgeMeshTraits>();
  template
  BaselineMesh generate_spheroid<BaselineMeshTraits>(uint);
  template
  FNormalMesh generate_spheroid<FNormalMeshTraits>(uint);
  template
  HalfedgeMesh generate_spheroid<HalfedgeMeshTraits>(uint);
  
  // generate_convex_hull_approx
  template
  BaselineMesh generate_convex_hull_approx<BaselineMeshTraits, eig::Array3f>(std::span<const eig::Array3f>, const BaselineMesh &);
  template
  FNormalMesh generate_convex_hull_approx<FNormalMeshTraits, eig::Array3f>(std::span<const eig::Array3f>, const FNormalMesh &);
  template
  HalfedgeMesh generate_convex_hull_approx<HalfedgeMeshTraits, eig::Array3f>(std::span<const eig::Array3f>, const HalfedgeMesh &);
  template
  BaselineMesh generate_convex_hull_approx<BaselineMeshTraits, eig::AlArray3f>(std::span<const eig::AlArray3f>, const BaselineMesh &);
  template
  FNormalMesh generate_convex_hull_approx<FNormalMeshTraits, eig::AlArray3f>(std::span<const eig::AlArray3f>, const FNormalMesh &);
  template
  HalfedgeMesh generate_convex_hull_approx<HalfedgeMeshTraits, eig::AlArray3f>(std::span<const eig::AlArray3f>, const HalfedgeMesh &);

  template
  HalfedgeMesh generate_convex_hull<HalfedgeMeshTraits, eig::Array3f>(std::span<const eig::Array3f>);
  template
  std::pair<std::vector<eig::Array3f>, std::vector<eig::Array3u>> generate_convex_hull<eig::Array3f>(std::span<const eig::Array3f>);
  template
  std::pair<std::vector<eig::AlArray3f>, std::vector<eig::Array3u>> generate_convex_hull<eig::AlArray3f>(std::span<const eig::AlArray3f>);

  // simplify
  template
  BaselineMesh simplify<BaselineMeshTraits>(const BaselineMesh &, const BaselineMesh &, uint);
  template
  FNormalMesh simplify<FNormalMeshTraits>(const FNormalMesh &, const FNormalMesh &, uint);
  template
  HalfedgeMesh simplify<HalfedgeMeshTraits>(const HalfedgeMesh &, const HalfedgeMesh &, uint);
  template
  BaselineMesh simplify_edges<BaselineMeshTraits>(const BaselineMesh &, float);
  template
  FNormalMesh simplify_edges<FNormalMeshTraits>(const FNormalMesh &, float);
  template
  HalfedgeMesh simplify_edges<HalfedgeMeshTraits>(const HalfedgeMesh &, float);

  /* // Generate barycentric weights
  template
  std::vector<eig::ArrayXf> generate_barycentric_weights(const HalfedgeMesh &, std::span<const eig::Array3f>);
  template
  std::vector<eig::ArrayXf> generate_barycentric_weights(const HalfedgeMesh &, std::span<const eig::AlArray3f>); */
} // namespace met