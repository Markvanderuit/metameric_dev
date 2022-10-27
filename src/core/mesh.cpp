#include <metameric/core/mesh.hpp>
#include <metameric/core/utility.hpp>
#include <metameric/core/detail/trace.hpp>
#include <fmt/ranges.h>
#include <algorithm>
#include <execution>
#include <ranges>
#include <unordered_set>
#include <unordered_map>
#include <vector>

namespace met {
  namespace detail {
    // Hash and key_equal for eigen types for std::unordered_map insertion
    template <typename T>
    constexpr auto eig_hash = [](const auto &mat) {
      size_t seed = 0;
      for (size_t i = 0; i < mat.size(); ++i) {
        auto elem = *(mat.data() + i);
        seed ^= std::hash<T>()(elem) + 0x9e3779b9 + (seed << 6) + (seed >> 2);
      }
      return seed;
    };
    constexpr auto eig_equal = [](const auto &a, const auto &b) { return a.isApprox(b); };
  } // namespace detail

  template <typename T, typename E>
  IndexedMesh<T, E>::IndexedMesh(std::span<const Vert> verts, std::span<const Elem> elems)
  : m_verts(range_iter(verts)), m_elems(range_iter(elems)) { }

  template <typename T, typename E>
  IndexedMesh<T, E>::IndexedMesh(const HalfEdgeMesh<T> &other) {
    // Allocate record space
    m_verts.resize(other.verts().size());
    m_elems.resize(other.faces().size());

    // Initialize vertex positions
    std::transform(std::execution::par_unseq, range_iter(other.verts()), m_verts.begin(),
      [](const auto &v) { return v.p; });

    // Construct faces
    #pragma omp parallel for
    for (int i = 0; i < m_elems.size(); ++i) {
      auto verts = other.verts_around_face(i);
      for (int j = 0; j < verts.size(); j++)
        m_elems[i][j] = verts[j];
    }
  }

  template <typename T>
  IndexedMesh<T, eig::Array3u> generate_unit_sphere(uint n_subdivs) {
    met_trace();

    using Vt = IndexedMesh<T, eig::Array3u>::Vert;
    using El = IndexedMesh<T, eig::Array3u>::Elem;
    using VMap  = std::unordered_map<Vt, uint, 
                                     decltype(detail::eig_hash<float>), 
                                     decltype(detail::eig_equal)>;
    
    // Initial mesh describes an octahedron
    std::vector<Vt> vts = { Vt(-1.f, 0.f, 0.f ), Vt( 0.f,-1.f, 0.f ), Vt( 0.f, 0.f,-1.f ),
                            Vt( 1.f, 0.f, 0.f ), Vt( 0.f, 1.f, 0.f ), Vt( 0.f, 0.f, 1.f ) };
    std::vector<El> els = { El(0, 1, 2), El(3, 2, 1), El(0, 5, 1), El(3, 1, 5),
                            El(0, 4, 5), El(3, 5, 4), El(0, 2, 4), El(3, 4, 2) };

    // Perform loop subdivision several times
    for (uint d = 0; d < n_subdivs; ++d) {        
      std::vector<El> els_(4 * els.size()); // New elements are inserted in this larger vector
      VMap vmap(64, detail::eig_hash<float>, detail::eig_equal); // Identical vertices are first tested in this map

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

  template <typename T>
  IndexedMesh<T, eig::Array2u> generate_wireframe(const IndexedMesh<T, eig::Array3u> &input_mesh) {
    met_trace();

    IndexedMesh<T, eig::Array2u> mesh;
    mesh.verts() = input_mesh.verts();
    mesh.elems().reserve(input_mesh.elems().size() * 2);

    auto &elems = mesh.elems();
    for (auto &e : input_mesh.elems()) {
      const uint i = e.x(), j = e.y(), k = e.z();
      elems.push_back({ i, j });
      elems.push_back({ j, k });
      elems.push_back({ k, i });
    }

    return mesh;
  }

  /* Explicit template instantiations for common types */

  template class IndexedMesh<eig::Array3f, eig::Array3u>;
  template class IndexedMesh<eig::AlArray3f, eig::Array3u>;

  template IndexedMesh<eig::Array3f, eig::Array3u>   generate_unit_sphere<eig::Array3f>(uint);
  template IndexedMesh<eig::AlArray3f, eig::Array3u> generate_unit_sphere<eig::AlArray3f>(uint);

  template IndexedMesh<eig::Array3f, eig::Array2u>
  generate_wireframe<eig::Array3f>(const IndexedMesh<eig::Array3f, eig::Array3u> &);
  template IndexedMesh<eig::AlArray3f, eig::Array2u>
  generate_wireframe<eig::AlArray3f>(const IndexedMesh<eig::AlArray3f, eig::Array3u> &);
} // namespace met
