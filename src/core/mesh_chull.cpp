#include <metameric/core/mesh.hpp>
#include <metameric/core/utility.hpp>
#include <metameric/core/detail/trace.hpp>
#include <metameric/core/detail/openmesh.hpp>
#include <algorithm>
#include <execution>
#include <functional>
#include <ranges>
#include <unordered_map>
#include <unordered_set>
#include <meshoptimizer.h>

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

    template <typename T>
    using eig_hash_t  = decltype(eig_hash<T>);
    using eig_equal_t = decltype(eig_equal);

    template <typename T>
    struct Triangle {
      Triangle(const T &a, const T &b, const T &c) {
        normal = (b - a).matrix().cross((c - a).matrix() ).normalized();
        center = (c + b + a) / 3.f;
        vertices[0] = a;
        vertices[1] = b;
        vertices[2] = c;
      }

      std::array<T, 3> vertices;
      T                normal;
      T                center;
    };

    template <typename T>
    constexpr 
    auto triangle_hash = [](const Triangle<T> &t) {
      std::array<T, 3> sorted_v = t.vertices;
      std::ranges::sort(sorted_v, [](const auto &a, const auto &b) { return a.sum() < b.sum(); });
      return eig_hash<float>(sorted_v[0]) +
             eig_hash<float>(sorted_v[1]) +
             eig_hash<float>(sorted_v[2]) +
             eig_hash<float>(t.center);
    };

    template <typename T>
    constexpr 
    auto triangle_equal = [](const Triangle<T> a, const Triangle<T> &b) {
      std::array<T, 3> sorted_v_a = a.vertices;
      std::array<T, 3> sorted_v_b = b.vertices;
      std::ranges::sort(sorted_v_a, [](const auto &a, const auto &b) { return a.sum() < b.sum(); });
      std::ranges::sort(sorted_v_b, [](const auto &a, const auto &b) { return a.sum() < b.sum(); });
      return eig_equal(sorted_v_a[0], sorted_v_b[0]) &&
             eig_equal(sorted_v_a[1], sorted_v_b[1]) &&
             eig_equal(sorted_v_a[2], sorted_v_b[2]) &&
             eig_equal(a.center, b.center);
    };
  } // namespace detail

  template <typename T>
  omesh::BaseMesh from_indexed(const IndexedMesh<T, eig::Array3u> &o) {
    omesh::VMesh<T> mesh;

    std::vector<omesh::SmartVertexHandle> vt_handles(o.verts().size());
    std::transform(range_iter(o.verts()), vt_handles.begin(), [&mesh](const T &p) { 
      return mesh.add_vertex(p); 
    });
    std::for_each(range_iter(o.elems()), [&mesh, &vt_handles](const eig::Array3u &e) { 
      mesh.add_face(vt_handles[e[0]], vt_handles[e[1]], vt_handles[e[2]]);
    });

    return mesh;
  }
  
  template <typename T>
  IndexedMesh<T, eig::Array3u> from_omesh(const omesh::VMesh<T> &o) {
    std::vector<T>            verts(o.n_vertices());
    std::vector<eig::Array3u> elems(o.n_faces());

    std::transform(std::execution::par_unseq, range_iter(o.vertices()), verts.begin(), 
      [&o](auto vh) { return o.point(vh); });

    std::transform(std::execution::par_unseq, range_iter(o.faces()), elems.begin(),[](auto fh) {
      eig::Array3u el; 
      std::transform(range_iter(fh.vertices()), el.begin(), [](auto vh) { return vh.idx(); });
      return el;
    });

    return IndexedMesh<T, eig::Array3u>(verts, elems);
  }
  
  template <typename T>
  IndexedMesh<T, eig::Array3u> generate_convex_hull(const IndexedMesh<T, eig::Array3u> &sphere_mesh, std::span<const T> points, float threshold, float max_error) {
    met_trace();

    // Temporary placeholder for testing
    // return sphere_mesh;

    /* BEGIN here */
    /* https://stackoverflow.com/questions/19530731/qhull-library-c-interface */

    /* constexpr uint n_dims = 3;
    const char *input_comm = "";
    const char *qhull_comm = "Qt"; // Ask for triangulated output

    // Slow scatter to avoid padding: TODO replace with something smarter, or convert to unpadded input beforehand?
    // Needs benchmarking
    std::vector<double> points_d(points.size() * n_dims);
    #pragma omp parallel for
    for (int i = 0; i < points_d.size(); ++i)
      points_d[i] = static_cast<double>(points[i / 3][i % 3]);

    // Call Qhull to generate convex hull mesh
    orgQhull::Qhull qhull;
    qhull.runQhull("", n_dims, points.size(), points_d.data(), qhull_comm);

    // Allocate memory in output format
    std::vector<T> verts;
    verts.reserve(qhull.vertexCount());
    std::vector<eig::Array3u> elems(qhull.facetCount());

    auto qh_verts = qhull.vertexList().toStdVector();
    auto qh_faces = qhull.facetList().toStdVector();

    uint map_c = 0;
    std::unordered_map<T, uint, detail::eig_hash_t<float>, detail::eig_equal_t> vertex_map;
    
    constexpr auto facet_to_eig = [](const orgQhull::QhullVertex &v) {
      T _v;
      double *coords = v.point().coordinates();
      std::copy(coords, coords + n_dims, _v.begin());
      return _v;
    };

    std::for_each(range_iter(qh_verts), [&](const orgQhull::QhullVertex &_v) {
      T v = facet_to_eig(_v);
      if (auto it = vertex_map.find(v); it == vertex_map.end()) {
        verts.push_back(v);
        vertex_map[v] = map_c++;
      }
    });
    
    std::transform(std::execution::par_unseq, range_iter(qh_faces), elems.begin(), [&](const orgQhull::QhullFacet &f) {
      eig::Array3u el;
      for (uint i = 0; i < 3; ++i) {
        T v = facet_to_eig(f.vertices()[i]);
        el[i] = vertex_map[v];
      }
      return el;
    });

    auto m = IndexedMesh<T, eig::Array3u>(verts, elems);
    return m; */

    /* STOP here */

    IndexedMesh<T, eig::Array3u> mesh = sphere_mesh;

    /* BEGIN HERE */

    /* std::unordered_set<
      detail::Triangle<T>, 
      decltype(detail::triangle_hash<T>), 
      decltype(detail::triangle_equal<T>)
    > triangle_set;

    std::for_each(range_iter(mesh.elems()), [&](auto &e) {
      // Obtain projected vertices
      std::array<T, 3> verts = { mesh.verts()[e[0]], 
                                 mesh.verts()[e[1]], 
                                 mesh.verts()[e[2]] };
      std::for_each(range_iter(verts), [&](auto &v) {
        // Obtain a range of point projections along this line
        auto proj_funct = [&](const auto &p) { 
          auto p_ = (p).matrix();
          return v.matrix().dot(p_); 
        };
        auto proj_range = points | std::views::transform(proj_funct);

        // Find iterator to endpoint, given these point projections
        auto proj_maxel = std::ranges::max_element(proj_range);
        
        // Replace mesh vertex with this endpoint
        v = points[std::distance(proj_range.begin(), proj_maxel)];
      });
      
      // Test for existence of this triangle
      detail::Triangle<T> triangle(verts[0], verts[1], verts[2]);
      triangle_set.insert(triangle);
    }); */

    /* END HERE */

    /* // Convert to (badly) indexed mesh and return
    std::vector<detail::Triangle<T>> triangles(range_iter(triangle_set));
    std::vector<T>                   verts(triangle_set.size() * 3);
    std::vector<eig::Array3u>        elems(triangle_set.size());

    #pragma omp parallel for
    for (int i = 0; i < triangles.size(); ++i) {
      const detail::Triangle<T> &t = triangles[i];
      verts[i * 3 + 0] = t.vertices[0];
      verts[i * 3 + 1] = t.vertices[1];
      verts[i * 3 + 2] = t.vertices[2];
      elems[i]         = eig::Array3u(i * 3 + 0, 
                                      i * 3 + 1, 
                                      i * 3 + 2);
    }

    // Compute centroid of point set
    constexpr auto eig_add = [](const auto &a, const auto &b) { return (a + b).eval(); };
    T center = std::reduce(std::execution::par_unseq, range_iter(verts), T(0.f), eig_add) 
              / static_cast<float>(verts.size());
    
    fmt::print("{}\n", center);

    // Flip incorretly ordered triangles
    #pragma omp parallel for
    for (int i = 0; i < triangles.size(); ++i) {
      const detail::Triangle<T> &t = triangles[i];
      auto d = (t.center - center).matrix().normalized();
      if (t.normal.matrix().dot(d) <= 0.f) {
        std::swap(elems[i][1], elems[i][2]);
      }
    }

    mesh = IndexedMesh<T>(verts, elems); */

    // std::unordered_set<eig::Array3u, detail::eig_hash_t<uint>, detail::eig_equal_t> double_face_set;


   /*  std::for_each(std::execution::par_unseq, range_iter(mesh.elems()), [&](auto &v) {
      T &u = mesh.verts()[e[0]], &v = mesh.verts()[e[1]], &w = mesh.verts()[e[2]];


    }); */

    // Compute centroid of point set
    constexpr auto f_add = [](const auto &a, const auto &b) { return (a + b).eval(); };
    T centroid = std::reduce(std::execution::par_unseq, range_iter(points), T(0.f), f_add) 
              / static_cast<float>(points.size());
    fmt::print("{}\n", centroid);

    // For each vertex in mesh, each defining a unit direction and therefore line through the origin:
    std::for_each(std::execution::par_unseq, range_iter(mesh.verts()), [&](auto &v) {
      // Obtain a range of point projections along this line
      auto proj_funct = [&](const auto &p) { 
        auto p_ = (p).matrix();
        return v.matrix().dot(p_); 
      };
      auto proj_range = points | std::views::transform(proj_funct);

      // Find iterator to endpoint, given these point projections
      auto proj_maxel = std::ranges::max_element(proj_range);
      
      // Replace mesh vertex with this endpoint
      v = points[std::distance(proj_range.begin(), proj_maxel)];
    });

    // clean_stitch_vertices(mesh);

  //   // Detect double faces
  //   std::unordered_set<eig::Array3u, detail::eig_hash_t<uint>, detail::eig_equal_t> double_face_set;
  //   uint double_count = 0;
  //   for (uint i = 0; i < mesh.elems().size(); ++i) {
  //     auto &el = mesh.elems()[i];
  //     auto sorted_el = el;
  //     std::ranges::sort(sorted_el);
  //     if (auto it = double_face_set.find(sorted_el); it != double_face_set.end()) {
  //       el = 0;
  //       double_count++;
  //     } else {
  //       double_face_set.emplace(sorted_el);
  //     }
  //   }
  //   fmt::print("Double count: {}\n", double_count);

  //   // Flip inward-facing triangles
  //   uint flip_count = 0;
  //   std::for_each(std::execution::par_unseq, range_iter(mesh.elems()), [&](auto &e) {
  //     const T &u = mesh.verts()[e[0]], &v = mesh.verts()[e[1]], &w = mesh.verts()[e[2]];

  //     // Test for already collapsed vertex 
  //     T c = (u + v + w) / 3.f;
  //     if (c.isApprox(u)) {
  //       return;
  //     }

  //     // Face normal vector, and vector from centroid to face 
  //     T n  = T(-(v - u).matrix().cross((w - u).matrix()).normalized().eval());
  //     T n_ = (c - centroid).matrix().normalized();

  //     // Test for inward facing triangle
  //     if (n.matrix().dot(n_.matrix()) >= 0) {
  //       // e = 0;
  //     } else {
  //       std::swap(e[0], e[1]);
  //       flip_count++;
  //     }

  //     // guard(n.matrix().dot(n_.matrix()) >= 0);

  //     // Forcibly collapse triangle to outermost point
  //     /* e = (u - c).matrix().squaredNorm() > (v - c).matrix().squaredNorm()
  //       ? e[0]
  //       : ((v - c).matrix().squaredNorm() > (w - c).matrix().squaredNorm()
  //       ? e[1]
  //       : e[2]); */
  //     // e = e[0];

  //     // Forcibly flip triangle
  //     // std::swap(e[0], e[1]);

  //     // Forcibly destroy triangle
  //     // e = 0;
  //   });
  //   fmt::print("Flip count: {}\n", flip_count);

  //   // float threshold = 0.05f;
  //   // float max_error = 0.5f;

    return mesh;

    /* size_t target_index_count = size_t((mesh.elems().size() * 3) * threshold);
    std::vector<uint> elems_target(mesh.elems().size() * 3);

    auto elems_src = cnt_span<const uint>(mesh.elems());
    auto verts_src = cnt_span<const float>(mesh.verts());

    size_t new_index_count = meshopt_simplify(elems_target.data(), elems_src.data(), elems_src.size(), verts_src.data(), 
      mesh.verts().size(), sizeof(T), target_index_count, max_error, 0, nullptr);

    fmt::print("Simplify: {} -> {}\n", elems_target.size(), new_index_count);
    elems_target.resize(new_index_count);

    return IndexedMesh<T, eig::Array3u>(mesh.verts(), cnt_span<const eig::Array3u>(elems_target)); */
  }
  
  template <typename T>
  IndexedMesh<T, eig::Array3u> generate_convex_hull(std::span<const T> points) {
    met_trace();
    return generate_convex_hull<T>(generate_unit_sphere<T>(), points, 0.05, 0.5);
  }

  template <typename T>
  bool is_point_inside_convex_hull(const IndexedMesh<T, eig::Array3u> &chull, const T &test_point) {
    met_trace();

    /*
      Algorithm: test if the point lies inside the convex hull, and otherwise return the
      nearest point that lies on (or slightly inside) the convex hull.

      Steps:
      0. Compute the hull's centroid
      1. For each point on the hull...
        1. Compute the normal vector given the hull's centroid
        2. Test if the test point lies above the plane defined by this normal vector
      2. If 1. returns no points, return the test point
      3. Else, find the three closest unique points for which 1. tests true
      4. Define a plane given these three points and compute its face normal
      5. Project the test point on to this plane, and return this projected point
    */

    T center = chull.centroid();
    
    // Compute triangle normals and centroids
    std::vector<T> face_normals(chull.elems().size()), face_centroids(chull.elems().size());
    std::transform(std::execution::par_unseq, range_iter(chull.elems()), face_centroids.begin(), 
    [&](const eig::AlArray3u &el) {
      const T &a = chull.verts()[el[0]], &b = chull.verts()[el[1]], &c = chull.verts()[el[2]];
      return ((c + b + a) / 3.f).eval();
    });
    std::transform(std::execution::par_unseq, range_iter(chull.elems()), face_normals.begin(), 
    [&](const eig::AlArray3u &el) {
      const T &a = chull.verts()[el[0]], &b = chull.verts()[el[1]], &c = chull.verts()[el[2]];

      // Test for collapsed triangles
      if (a.isApprox(b) || a.isApprox(c) || b.isApprox(c))
        return T(0.f);

      return T(-(b - a).matrix().cross((c - a).matrix()).normalized().eval());
    });

    // Project on to every triangle plane
    std::vector<float> dot_prod(chull.elems().size(), 0.f);
    #pragma omp parallel for
    for (int i = 0; i < dot_prod.size(); ++i) {
      const T &n = face_normals[i], &c = face_centroids[i];
      
      // Skip collapsed triangles
      guard_continue(!n.isZero());

      // Skip inwards-facing triangles
      guard_continue((c - center).matrix().normalized().dot(n.matrix()) >= 0.f);

      const T v = (test_point - c).matrix().normalized().eval();
      dot_prod[i] = v.matrix().dot(n.matrix());
    }

    // float d = *it;
    // fmt::print("x = {}, p = {}, n = {}, d = {}\n", 
    //   test_point,
    //   face_centroids[std::distance(dot_prod.begin(), it)],
    //   face_normals[std::distance(dot_prod.begin(), it)],
    //   d);

    uint positive_count = 0;
    for (uint i = 0; i < dot_prod.size(); ++i) {
      if (dot_prod[i] > 0.f)
        positive_count++;
    }
    fmt::print("positive {}\n", positive_count);
    // fmt::print("max d is {} given {}\n", d, face_centroids[std::distance(dot_prod.begin(), it)]);
    

    // Find maximum element in dot_prod, test if it is positive
    auto it = std::max_element(std::execution::par_unseq, range_iter(dot_prod));
    return (*it <= 0.f);
  }

  /* Explicit template instantiations for common types */

  template IndexedMesh<eig::Array3f, eig::Array3u> 
  generate_convex_hull<eig::Array3f>(const IndexedMesh<eig::Array3f, eig::Array3u> &, std::span<const eig::Array3f>, float, float);
  template IndexedMesh<eig::AlArray3f, eig::Array3u> 
  generate_convex_hull<eig::AlArray3f>(const IndexedMesh<eig::AlArray3f, eig::Array3u> &, std::span<const eig::AlArray3f>, float, float);
                                       
  template IndexedMesh<eig::Array3f, eig::Array3u> 
  generate_convex_hull<eig::Array3f>(std::span<const eig::Array3f>);
  template IndexedMesh<eig::AlArray3f, eig::Array3u> 
  generate_convex_hull<eig::AlArray3f>(std::span<const eig::AlArray3f>);  

  template bool
  is_point_inside_convex_hull(const IndexedMesh<eig::Array3f, eig::Array3u> &chull, const eig::Array3f &);
  template bool
  is_point_inside_convex_hull(const IndexedMesh<eig::AlArray3f, eig::Array3u> &chull, const eig::AlArray3f &);
} // namespace metameric