#include <metameric/core/mesh.hpp>
#include <metameric/core/utility.hpp>
#include <metameric/core/detail/trace.hpp>
#include <algorithm>
#include <execution>
#include <ranges>
#include <unordered_map>
#include <vector>

namespace met {
  namespace detail {
    // Hash and key_equal for eigen types for std::unordered_map insertion
    constexpr auto matrix_hash = [](const auto &mat) {
      size_t seed = 0;
      for (size_t i = 0; i < mat.size(); ++i) {
        auto elem = *(mat.data() + i);
        seed ^= std::hash<float>()(elem) + 0x9e3779b9 + (seed << 6) + (seed >> 2);
      }
      return seed;
    };
    constexpr auto matrix_equal = [](const auto &a, const auto &b) { return a.isApprox(b); };
  } // namespace detail

  Mesh generate_unit_sphere(uint n_subdivs) {
    met_trace();

    using Vt = Mesh::VertType;
    using El = Mesh::ElemType;
    using VMap  = std::unordered_map<Vt, uint, 
                                      decltype(detail::matrix_hash), 
                                      decltype(detail::matrix_equal)>;
    
    // Initial mesh describes an octahedron
    std::vector<Vt> vts = { Vt(-1.f, 0.f, 0.f ), Vt( 0.f,-1.f, 0.f ), Vt( 0.f, 0.f,-1.f ),
                            Vt( 1.f, 0.f, 0.f ), Vt( 0.f, 1.f, 0.f ), Vt( 0.f, 0.f, 1.f ) };
    std::vector<El> els = { El(0, 1, 2), El(3, 2, 1), El(0, 5, 1), El(3, 1, 5),
                            El(0, 4, 5), El(3, 5, 4), El(0, 2, 4), El(3, 4, 2) };

    // Perform loop subdivision several times
    for (uint d = 0; d < n_subdivs; ++d) {        
      std::vector<El> els_(4 * els.size()); // New elements are inserted in this larger vector
      VMap vmap(64, detail::matrix_hash, detail::matrix_equal); // Identical vertices are first tested in this map

      #pragma omp parallel for
      for (int e = 0; e < els.size(); ++e) {
        // Old and new vertex indices
        eig::Array3u ijk = els[e], abc;
        
        // Compute edge midpoints, lifted to the unit sphere
        std::array<Vt, 3> new_vts = { (vts[ijk[0]] + vts[ijk[1]]).matrix().normalized(),
                                      (vts[ijk[1]] + vts[ijk[2]]).matrix().normalized(),
                                      (vts[ijk[2]] + vts[ijk[0]]).matrix().normalized() };

        // Inside critical section, insert lifted edge midpoints and set new vertex indices
        // if they don't exist already on a neighbouring triangle
        #pragma omp critical
        for (uint i = 0; i < abc.size(); ++i) {
          if (auto it = vmap.find(new_vts[i]); it != vmap.end()) {
            abc[i] = it->second;
          } else {
            abc[i] = vts.size();
            vts.push_back(new_vts[i]);
            vmap.emplace(new_vts[i], abc[i]);
          }
        }
      
        // Create and insert newly subdivided elements
        const auto new_els = { El(ijk[0], abc[0], abc[2]), El(ijk[1], abc[1], abc[0]), 
                                El(ijk[2], abc[2], abc[1]), El(abc[0], abc[1], abc[2]) };
        std::ranges::copy(new_els, els_.begin() + 4 * e);
      }

      els = els_; // Overwrite list of elements with new subdivided list
    }

    return { std::move(vts), std::move(els) };
  }

  Mesh generate_convex_hull(const Mesh &sphere_mesh,
                            const std::vector<eig::Array3f> &points) {
    met_trace();

    Mesh mesh = sphere_mesh;

    // For each vertex in mesh, each defining a line through the origin:
    std::for_each(std::execution::par_unseq, range_iter(mesh.vertices), [&](auto &v) {
      // Obtain a range of point projections along this line
      auto proj_funct  = [&v](const auto &p) { return v.matrix().dot(p.matrix()); };
      auto proj_range = points | std::views::transform(proj_funct);

      // Find iterator to endpoint, given projections
      auto it = std::ranges::max_element(proj_range);

      // Replace mesh vertex with this endpoint
      v = *(points.begin() + std::distance(proj_range.begin(), it));
    });

    /* // Find and erase collapsed triangles
    fmt::print("pre_erase {}\n", mesh.elements.size());
    std::erase_if(mesh.elements, [&](const auto &e) {
      const uint i = e.x(), j = e.y(), k = e.z();
      return (mesh.vertices[i].isApprox(mesh.vertices[j])) ||
              (mesh.vertices[j].isApprox(mesh.vertices[k])) ||
              (mesh.vertices[k].isApprox(mesh.vertices[i]));
    });
    fmt::print("post_erase {}\n", mesh.elements.size()); */

    // Find and erase inward-pointing triangles
    /* std::erase_if(mesh.elements, [&](const auto &e) {
    }); */

    return mesh;
  }
  
  Mesh generate_convex_hull(const std::vector<eig::Array3f> &points) {
    met_trace();
    return generate_convex_hull(generate_unit_sphere(), points);
  }
} // namespace met
