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

    // Compute triangle normals and centroids
    std::vector<T> face_normals(chull.elems().size()), face_centroids(chull.elems().size());
    std::transform(std::execution::par_unseq, range_iter(chull.elems()), face_normals.begin(), 
    [&](const eig::AlArray3u &el) {
      const T &a = chull.verts()[el[0]], &b = chull.verts()[el[1]], &c = chull.verts()[el[2]];
      if (a.isApprox(b) || a.isApprox(c) || b.isApprox(c))
        return T(0.f);
      return T(-(b - a).matrix().cross((c - a).matrix()).normalized().eval());
    });
    std::transform(std::execution::par_unseq, range_iter(chull.elems()), face_centroids.begin(), 
    [&](const eig::AlArray3u &el) {
      const T &a = chull.verts()[el[0]], &b = chull.verts()[el[1]], &c = chull.verts()[el[2]];
      return ((c + b + a) / 3.f).eval();
    });

    // Project on to every triangle plane
    std::vector<float> dot_prod(chull.elems().size(), 0.f);
    #pragma omp parallel for
    for (int i = 0; i < dot_prod.size(); ++i) {
      const T &n = face_normals[i];
      guard_continue(!n.isZero());
      const T v = (test_point - face_centroids[i]).matrix().normalized().eval();
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
    // fmt::print("max d is {} given {}\n", d, face_centroids[std::distance(dot_prod.begin(), it)]);
    
    // Test if point lies inside convex hull
    if (d <= 0.f) {
      return test_point;
    }

    // Return closest point on/inside convex hull
    return face_centroids[std::distance(dot_prod.begin(), it)];

    /* // Compute centroid of convex hull
    constexpr auto f_add = [](const T &a, const T &b) { return (a + b).eval(); };
    T c = std::reduce(std::execution::par_unseq, range_iter(hull_points), T(0.f), f_add) 
        / static_cast<float>(hull_points.size());
    
    std::vector<float> dot_prod(hull_points.size());
    #pragma omp parallel for
    for (int i = 0; i < hull_points.size(); ++i) {
      const T &x = hull_points[i];
      const T n = (x - c).matrix().normalized();
      const T v = (test_point - x).matrix().normalized();
      dot_prod[i] = v.matrix().dot(n.matrix());
    }

    // Find maximum element in dot_prod
    auto it = std::max_element(std::execution::par_unseq, range_iter(dot_prod));
    float d = *it;
    
    // Test if point lies inside convex hull
    fmt::print("max dot: {}\n", d);
    if (d <= 0.f) {
      return test_point;
    }

    // Return closest point on/inside convex hull
    return hull_points[std::distance(dot_prod.begin(), it)];  */
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