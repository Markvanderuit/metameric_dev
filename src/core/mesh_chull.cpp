#include <metameric/core/mesh.hpp>
#include <metameric/core/utility.hpp>
#include <metameric/core/detail/trace.hpp>
#include <algorithm>
#include <execution>
#include <ranges>
#include <unordered_map>
#include "libqhullcpp/Qhull.h"
#include "libqhullcpp/QhullVertex.h"
#include "libqhullcpp/QhullVertexSet.h"
#include "libqhullcpp/QhullPoint.h"

namespace met {
  namespace detail {
    // key_hash for eigen types for std::unordered_map/unordered_set
    template <typename T>
    constexpr auto eig_hash = [](const auto &mat) {
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
  }
  
  template <typename T>
  IndexedMesh<T, eig::Array3u> generate_convex_hull(const IndexedMesh<T, eig::Array3u> &sphere_mesh, std::span<const T> points) {
    met_trace();

    /* BEGIN here */
    /* https://stackoverflow.com/questions/19530731/qhull-library-c-interface */

    constexpr uint n_dims = 3;
    const char *input_comm = "";
    const char *qhull_comm = "Qt Qx"; // Ask for triangulated output

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

    fmt::print("Result is {} verts, {} faces\n", qh_verts.size(), qh_faces.size());

    uint map_c = 0;
    std::unordered_map<T, uint, detail::eig_hash_t<float>, detail::eig_equal_t> vertex_map;
    
    constexpr auto facet_to_eig = [](const orgQhull::QhullVertex &v) {
      T _v;
      double *coords = v.point().coordinates();
      std::copy(coords, coords + n_dims, _v.begin());
      return _v;
    };
    
    std::transform(range_iter(qh_faces), elems.begin(), [&](const orgQhull::QhullFacet &f) {
      eig::Array3u el;
      for (uint i = 0; i < 3; ++i) {
        T v = facet_to_eig(f.vertices()[i]);
        if (auto it = vertex_map.find(v); it != vertex_map.end()) {
          el[i] = it->second;
        } else {
          el[i] = map_c;
          verts.push_back(v);
          vertex_map[v] = map_c++;
        }
      }
      return el;
    });

    fmt::print("Output is {} verts, {} faces\n", verts.size(), elems.size());

    // std::transform(range_iter(qh_verts), verts.begin(), [](const orgQhull::QhullVertex &v) {
    //   T _v;
    //   for (uint i = 0; i < n_dims; ++i)
    //     _v[i] = static_cast<float>(v.point().coordinates()[i]);
    //   if 
    //   return _v;
    // });


    // std::transform(range_iter(qh_faces), elems.begin(), [](const orgQhull::QhullFacet &f) {
    //   eig::Array3u el;
    //   for (uint i = 0; i < 3; ++i)
    //     el[i] = f.vertices()[i].id() - 1;
    //   return el;
    // });

    auto m = IndexedMesh<T, eig::Array3u>(verts, elems);
    clean_fix_winding_order(m);
    return m;

    // Extract mesh
    IndexedMesh<T, eig::Array3u> hull_mesh;

    /* STOP here */

    IndexedMesh<T, eig::Array3u> mesh = sphere_mesh;

    // For each vertex in mesh, each defining a line through the origin:
    std::for_each(std::execution::par_unseq, range_iter(mesh.verts()), [&](auto &v) {
      // Obtain a range of point projections along this line
      auto proj_funct = [&v](const auto &p) { return (v).matrix().dot(p.matrix()); };
      auto proj_range = points | std::views::transform(proj_funct);

      // Find iterator to endpoint, given projections
      auto it = std::ranges::max_element(proj_range);
      
      // Replace mesh vertex with this endpoint
      v = points[std::distance(proj_range.begin(), it)];
    });

    return mesh;
  }
  
  template <typename T>
  IndexedMesh<T, eig::Array3u> generate_convex_hull(std::span<const T> points) {
    met_trace();
    return generate_convex_hull<T>(generate_unit_sphere<T>(), points);
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
    fmt::print("{}\n", positive_count);
    // fmt::print("max d is {} given {}\n", d, face_centroids[std::distance(dot_prod.begin(), it)]);
    

    // Find maximum element in dot_prod, test if it is positive
    auto it = std::max_element(std::execution::par_unseq, range_iter(dot_prod));
    return (*it <= 0.f);
  }

  /* Explicit template instantiations for common types */

  template IndexedMesh<eig::Array3f, eig::Array3u> 
  generate_convex_hull<eig::Array3f>(const IndexedMesh<eig::Array3f, eig::Array3u> &, std::span<const eig::Array3f>);
  template IndexedMesh<eig::AlArray3f, eig::Array3u> 
  generate_convex_hull<eig::AlArray3f>(const IndexedMesh<eig::AlArray3f, eig::Array3u> &, std::span<const eig::AlArray3f>);
                                       
  template IndexedMesh<eig::Array3f, eig::Array3u> 
  generate_convex_hull<eig::Array3f>(std::span<const eig::Array3f>);
  template IndexedMesh<eig::AlArray3f, eig::Array3u> 
  generate_convex_hull<eig::AlArray3f>(std::span<const eig::AlArray3f>);  

  template bool
  is_point_inside_convex_hull(const IndexedMesh<eig::Array3f, eig::Array3u> &chull, const eig::Array3f &);
  template bool
  is_point_inside_convex_hull(const IndexedMesh<eig::AlArray3f, eig::Array3u> &chull, const eig::AlArray3f &);
} // namespace metameric