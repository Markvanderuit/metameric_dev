#include <metameric/core/mesh.hpp>
#include <metameric/core/utility.hpp>
#include <metameric/core/detail/trace.hpp>
#include <algorithm>
#include <execution>
#include <ranges>

namespace met {
  template <typename T>
  IndexedMesh<T, eig::Array3u> generate_convex_hull(const IndexedMesh<T, eig::Array3u> &sphere_mesh,
                                                    std::span<const T> points) {
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

  /* Explicit template instantiations for common types */

  template IndexedMesh<eig::Array3f, eig::Array3u> 
  generate_convex_hull<eig::Array3f>(const IndexedMesh<eig::Array3f, eig::Array3u> &sphere_mesh,
                                     std::span<const eig::Array3f>);
  template IndexedMesh<eig::AlArray3f, eig::Array3u> 
  generate_convex_hull<eig::AlArray3f>(const IndexedMesh<eig::AlArray3f, eig::Array3u> &sphere_mesh,
                                       std::span<const eig::AlArray3f>);
                                       
  template IndexedMesh<eig::Array3f, eig::Array3u> 
  generate_convex_hull<eig::Array3f>(std::span<const eig::Array3f>);
  template IndexedMesh<eig::AlArray3f, eig::Array3u> 
  generate_convex_hull<eig::AlArray3f>(std::span<const eig::AlArray3f>);  
} // namespace metameric