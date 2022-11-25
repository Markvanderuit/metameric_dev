#include <metameric/core/mesh.hpp>
#include <metameric/core/utility.hpp>
#include <metameric/core/detail/trace.hpp>
#include <algorithm>
#include <execution>
#include <ranges>
#include <unordered_set>
#include <unordered_map>
#include <vector>

namespace met {
  namespace detail {
    template <typename T>
    bool elements_share_edge(const T &a, const T &b) {
      uint num_eq = 0;
      for (uint i = 0; i < 3; ++i)
        for (uint j = 0; j < 3; ++j)
          num_eq += (a[i] == b[j]) ? 1 : 0;
      return num_eq > 1;
    }

    template <typename T>
    bool elements_falsely_wind(const T &a, const T &b) {
      for (uint i = 0; i < 3; ++i) {
        uint _i = (i + 1) % 3;
        for (uint j = 0; j < 3; ++j) {
          uint _j = (j + 1) % 3;
          if (a[j] == b[i] && a[_j] == b[_i])
            return true;
        }
      }
      return false;
    }

    template <typename T>
    std::pair<uint, uint> falsely_wound_indices(const T &base, const T &next) {
      for (uint i = 0; i < 3; ++i) {
        uint _i = (i + 1) % 3;
        for (uint j = 0; j < 3; ++j) {
          uint _j = (j + 1) % 3;
          if (base[j] == next[i] && base[_j] == next[_i])
            return { i, _i };
        }
      }
      return { 0, 0 };
    }

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
  } // namespace detail 
  
  template <typename T>
  void clean_stitch_vertices(IndexedMesh<T, eig::Array3u> &mesh) {
    met_trace();

    using vert_map = std::unordered_map<T, uint, detail::eig_hash_t<float>, detail::eig_equal_t>;
    vert_map vertex_id_map(16, detail::eig_hash<float>, detail::eig_equal);

    // For each vertex in each element
    for (auto &el : mesh.elems()) {
      for (auto &i : el) {
        const auto &vt = mesh.verts()[i];

        // If this vertex is already in the map with a different index....
        if (auto it = vertex_id_map.find(vt); it != vertex_id_map.end()) {
          // Move index to this other vertex
          if (i != it->second) i = it->second;        
          // guard_continue(i != it->second);
        } else {
          // Otherwise, register vertex id
          vertex_id_map.emplace(vt, i);
        }
      }
    }
  }

  template <typename T>
  void clean_delete_unused_vertices(IndexedMesh<T, eig::Array3u> &mesh) {
    met_trace();

    // Do a usage count of all vertices
    std::vector<bool> vert_flag_erase(mesh.verts().size(), true);
    for (auto &el : mesh.elems())
      for (auto &i : el)
        vert_flag_erase[i] = false;
    
    // Perform an exclusive count of transformed usage count to get a new set of indices
    std::vector<uint> vertex_indx_new(mesh.verts().size());
    std::transform_inclusive_scan(std::execution::par_unseq, range_iter(vert_flag_erase),
      vertex_indx_new.begin(), std::plus<uint>(), [](bool b) { return b ? 0 : 1; });
    std::for_each(std::execution::par_unseq, range_iter(vertex_indx_new), [](uint &i) { i--; });
    
    // Apply new indices to current element set
    for (auto &el : mesh.elems())
      for (auto &i : el)
        i = vertex_indx_new[i];

    // Obtain indices of vertices to erase in reverse order
    std::vector<uint> vert_indx_erase;
    for (auto it = vert_flag_erase.rbegin(); it != vert_flag_erase.rend(); ++it)
      if (bool flag = *it; flag) 
        vert_indx_erase.push_back(std::distance(it, vert_flag_erase.rend()) - 1);

    // Erase marked vertices in reverse order
    for (uint i : vert_indx_erase) 
      mesh.verts().erase(mesh.verts().begin() + i);
  }

  template <typename T>
  void clean_delete_collapsed_elems(IndexedMesh<T, eig::Array3u> &mesh) {
    met_trace();

    std::vector<bool> elem_flag_erase(mesh.elems().size(), false);
    auto &verts = mesh.verts();
    #pragma omp parallel for
    for (int i = 0; i < mesh.elems().size(); ++i) {
      auto &el = mesh.elems()[i];
      if (verts[el[0]].isApprox(verts[el[1]]) ||
          verts[el[1]].isApprox(verts[el[2]]) ||
          verts[el[2]].isApprox(verts[el[0]]))
        elem_flag_erase[i] = true;
    }

    // Obtain indices of elements to erase in reverse order
    std::vector<uint> elem_indx_erase;
    for (auto it = elem_flag_erase.rbegin(); it != elem_flag_erase.rend(); ++it)
      if (bool flag = *it; flag) 
        elem_indx_erase.push_back(std::distance(it, elem_flag_erase.rend()) - 1);

    // Erase marked elements in reverse order
    for (uint i : elem_indx_erase) 
      mesh.elems().erase(mesh.elems().begin() + i);
  }

  template <typename T>
  void clean_delete_double_elems(IndexedMesh<T, eig::Array3u> &mesh) {
    met_trace();

    using elem_set = std::unordered_set<eig::Array3u, detail::eig_hash_t<uint>, detail::eig_equal_t>;
    elem_set elem_map(16, detail::eig_hash<uint>, detail::eig_equal);

    std::vector<bool> elem_flag_erase(mesh.elems().size(), false);
    for (uint i = 0; i < mesh.elems().size(); ++i) {
      // Get sorted version of triangle, independent of winding order
      auto el_sorted = mesh.elems()[i];
      std::ranges::sort(el_sorted);

      // Double element!
      if (elem_map.contains(el_sorted)) {
        elem_flag_erase[i] = true;
      } else {
        elem_map.insert(el_sorted);
      }
    }

    // Obtain indices of elements to erase in reverse order
    std::vector<uint> elem_indx_erase;
    for (auto it = elem_flag_erase.rbegin(); it != elem_flag_erase.rend(); ++it)
      if (bool flag = *it; flag) 
        elem_indx_erase.push_back(std::distance(it, elem_flag_erase.rend()) - 1);


    // Erase marked vertices in reverse order
    for (uint i : elem_indx_erase) 
      mesh.elems().erase(mesh.elems().begin() + i);
  }

  template <typename T>
  void clean_fix_winding_order(IndexedMesh<T, eig::Array3u> &mesh) {
    met_trace();
    
    constexpr auto pop_back = [](auto &c) {
      auto back = c.at(c.size() - 1);
      c.pop_back();
      return back;
    };

    // Data structures for work in progress; take a single arbitrary
    // triangle from the winding_queue and set it in the fixed container
    auto winding_queue = mesh.elems();
    auto winding_fixed = decltype(winding_queue)();
    winding_fixed.reserve(winding_queue.size());
    winding_fixed.push_back(pop_back(winding_queue));

    // Untils no triangles remain:
    while (!winding_queue.empty()) {
      bool found_next = false;
      eig::Array3u el_curr, el_next;

      // Find an arbitrary triangle adjacent to an already fixed triangle
      for (auto &el_fixed : winding_fixed) {
        auto it = std::find_if(range_iter(winding_queue),
          [&](const auto &el_next) { return detail::elements_share_edge(el_fixed, el_next); });
        guard_continue(it != winding_queue.end());
        
        el_curr    = el_fixed;
        el_next    = *it;
        found_next = true;
        winding_queue.erase(it);

        break;
      }
      debug::check_expr_rel(found_next, "Could not find next adjacent triangle");

      // Fix potential winding issues with this new triangle if they occur
      if (auto [a, b] = detail::falsely_wound_indices(el_curr, el_next); a != b)
        std::swap(el_next[a], el_next[b]);

      winding_fixed.push_back(el_next);
    }

    mesh.elems() = winding_fixed;

    // Emergency hack fix
    using edge = eig::Array2u;
    using edge_set = std::unordered_set<edge, detail::eig_hash_t<uint>, detail::eig_equal_t>;
    edge_set edge_map(16, detail::eig_hash<uint>, detail::eig_equal);
    for (auto &el : mesh.elems()) {
      for (uint i = 0; i < 3; ++i) {
        uint j = (i + 1) % 3;
        edge ed = { el[i], el[j] };
        if (edge_map.contains(ed)) {
          edge ned = { el[j], el[i] };
          fmt::print("{} : {} set to {}\n", el, ed, ned);
          std::swap(el[i], el[j]);
          edge_map.insert(edge { el[i], el[j] });
        } else {
          edge_map.insert(ed);
        }
      }
    }
  }

  template <typename T>
  void clean_all(IndexedMesh<T, eig::Array3u> &mesh) {
    met_trace();

    fmt::print("pre: {} - {}\n", mesh.verts().size(), mesh.elems().size());
    clean_delete_collapsed_elems(mesh);
    fmt::print("post collapsed elem removal: {} - {}\n", mesh.verts().size(), mesh.elems().size());
    clean_stitch_vertices(mesh);
    fmt::print("post vertex stitching: {} - {}\n", mesh.verts().size(), mesh.elems().size());
    clean_delete_double_elems(mesh);
    fmt::print("post double elem removal: {} - {}\n", mesh.verts().size(), mesh.elems().size());
    clean_delete_unused_vertices(mesh);
    fmt::print("post unused vertex removal: {} - {}\n", mesh.verts().size(), mesh.elems().size());
    clean_fix_winding_order(mesh);
  }

  /* Explicit template instantiations for common types */

  template void clean_stitch_vertices<eig::Array3f>(IndexedMesh<eig::Array3f, eig::Array3u> &);
  template void clean_stitch_vertices<eig::AlArray3f>(IndexedMesh<eig::AlArray3f, eig::Array3u> &);
  template void clean_delete_unused_vertices<eig::Array3f>(IndexedMesh<eig::Array3f, eig::Array3u> &);
  template void clean_delete_unused_vertices<eig::AlArray3f>(IndexedMesh<eig::AlArray3f, eig::Array3u> &);
  template void clean_delete_double_elems(IndexedMesh<eig::Array3f, eig::Array3u> &);
  template void clean_delete_double_elems(IndexedMesh<eig::AlArray3f, eig::Array3u> &);
  template void clean_delete_collapsed_elems<eig::Array3f>(IndexedMesh<eig::Array3f, eig::Array3u> &);
  template void clean_delete_collapsed_elems<eig::AlArray3f>(IndexedMesh<eig::AlArray3f, eig::Array3u> &);
  template void clean_fix_winding_order<eig::Array3f>(IndexedMesh<eig::Array3f, eig::Array3u> &);
  template void clean_fix_winding_order<eig::AlArray3f>(IndexedMesh<eig::AlArray3f, eig::Array3u> &);
  template void clean_all<eig::Array3f>(IndexedMesh<eig::Array3f, eig::Array3u> &);
  template void clean_all<eig::AlArray3f>(IndexedMesh<eig::AlArray3f, eig::Array3u> &);
} // namespace detail