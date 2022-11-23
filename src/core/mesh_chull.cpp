#include <metameric/core/mesh.hpp>
#include <metameric/core/utility.hpp>
#include <metameric/core/detail/trace.hpp>
#include <algorithm>
#include <execution>
#include <ranges>

namespace met {
  template <typename T>
  IndexedMesh<T, eig::Array3u> generate_convex_hull(const IndexedMesh<T, eig::Array3u> &sphere_mesh, std::span<const T> points) {
    met_trace();

    IndexedMesh<T, eig::Array3u> mesh = sphere_mesh;

    // For each vertex in mesh, each defining a line through the origin:
    std::for_each(std::execution::par_unseq, range_iter(mesh.verts()), [&](auto &v) {
      // Obtain a range of point projections along this line
      auto proj_funct = [&v](const auto &p) { return v.matrix().dot(p.matrix()); };
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
  T move_point_inside_convex_hull(const IndexedMesh<T, eig::Array3u> &chull, const T &test_point) {
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

    // Compute mesh centroid
    constexpr auto f_add = [](const auto &a, const auto &b) { return (a + b).eval(); };
    T center = std::reduce(std::execution::par_unseq, range_iter(chull.verts()), T(0.f), f_add) 
             / static_cast<float>(chull.verts().size());
    
    // Compute triangle normals and centroids
    std::vector<T> face_normals(chull.elems().size()),
                   face_centroids(chull.elems().size());
    
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

    // Find maximum element in dot_prod
    auto it = std::max_element(std::execution::par_unseq, range_iter(dot_prod));
    float d = *it;
    fmt::print("x = {}, p = {}, n = {}, d = {}\n", 
      test_point,
      face_centroids[std::distance(dot_prod.begin(), it)],
      face_normals[std::distance(dot_prod.begin(), it)],
      d);

    uint positive_count = 0;
    for (uint i = 0; i < dot_prod.size(); ++i) {
      if (dot_prod[i] > 0.f)
        positive_count++;
    }
    fmt::print("{}\n", positive_count);
    // fmt::print("max d is {} given {}\n", d, face_centroids[std::distance(dot_prod.begin(), it)]);
    
    // Test if point lies inside convex hull
    if (d <= 0.f) {
      return test_point;
    }

    // Return closest point on/inside convex hull
    return face_centroids[std::distance(dot_prod.begin(), it)];
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

  template eig::Array3f
  move_point_inside_convex_hull(const IndexedMesh<eig::Array3f, eig::Array3u> &chull, const eig::Array3f &);
  template eig::AlArray3f
  move_point_inside_convex_hull(const IndexedMesh<eig::AlArray3f, eig::Array3u> &chull, const eig::AlArray3f &);
} // namespace metameric