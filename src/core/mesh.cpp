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
    constexpr auto matrix_hash = [](const auto &mat) {
      size_t seed = 0;
      for (size_t i = 0; i < mat.size(); ++i) {
        auto elem = *(mat.data() + i);
        seed ^= std::hash<T>()(elem) + 0x9e3779b9 + (seed << 6) + (seed >> 2);
      }
      return seed;
    };
    constexpr auto matrix_equal = [](const auto &a, const auto &b) { return a.isApprox(b); };

    template <typename T>
    void half_mesh_erase_vert(HalfEdgeMesh<T> &mesh, uint i) {
      mesh.verts().erase(mesh.verts().begin() + i);
      std::for_each(std::execution::par_unseq, range_iter(mesh.halfs()),
        [&](auto &half) { guard(half.vert_i > i); half.vert_i--; });
    }
    
    template <typename T>
    void half_mesh_erase_face(HalfEdgeMesh<T> &mesh, uint i) {
      mesh.faces().erase(mesh.faces().begin() + i);
      std::for_each(std::execution::par_unseq, range_iter(mesh.halfs()),
        [&](auto &half) { guard(half.face_i > i); half.face_i--; });
    }
    
    template <typename T>
    void half_mesh_erase_half(HalfEdgeMesh<T> &mesh, uint i) {
      mesh.halfs().erase(mesh.halfs().begin() + i);
      std::for_each(std::execution::par_unseq, range_iter(mesh.verts()),
        [&](auto &vert) { guard(vert.half_i > i); vert.half_i--; });
      std::for_each(std::execution::par_unseq, range_iter(mesh.faces()),
        [&](auto &face) { guard(face.half_i > i); face.half_i--; });
      std::for_each(std::execution::par_unseq, range_iter(mesh.halfs()),
        [&](auto &half) {
          if (half.next_i > i) half.next_i--;
          if (half.prev_i > i) half.prev_i--;
          if (half.twin_i > i) half.twin_i--;
      });
    }
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
  HalfEdgeMesh<T>::HalfEdgeMesh(const IndexedMesh<T, eig::Array3u> other) {
    // Allocate record space
    m_verts.resize(other.verts().size());
    m_faces.resize(other.elems().size());

    // Initialize vertex positions
    std::transform(std::execution::par_unseq, range_iter(other.verts()), m_verts.begin(), 
      [&](const auto &p) { return Vert { p, 3 * static_cast<uint>(other.elems().size()) }; });
    
    // Map to perform half-edge connections
    using Edge  = eig::Array2u;
    using Edges = std::unordered_map<Edge, uint, 
                                     decltype(detail::matrix_hash<uint>), 
                                     decltype(detail::matrix_equal)>;
    Edges edge_map;

    for (uint face_i = 0; face_i < m_faces.size(); ++face_i) {
      const auto &el = other.elems()[face_i];
      std::array<Edge, 3> face = { Edge { el.x(), el.y() },
                                   Edge { el.y(), el.z() }, 
                                   Edge { el.z(), el.x() }};

      // Insert initial half-edge data for this face 
      for (uint i = 0; i < face.size(); ++i) {
        auto &curr_edge = face[i];
        m_halfs.push_back(Half { .vert_i = curr_edge.x(), .face_i = face_i });
        
        debug::check_expr_rel(!edge_map.contains(curr_edge));
        edge_map.insert({ curr_edge, static_cast<uint>(m_halfs.size() - 1) });
      }

      // Establish connections
      for (uint i = 0; i < face.size(); ++i) {
        // Find next/previous edges in face: j = next, k = prev
        const uint j = (i + 1) % face.size(), k = (j + 1) % face.size();
        auto &curr_edge = face[i], &next_edge = face[j], &prev_edge = face[k];

        // Find the current half edge and assign connections
        uint curr_i = edge_map[curr_edge];
        auto &curr_half  = m_halfs[curr_i];
        curr_half.next_i = edge_map[next_edge];
        curr_half.prev_i = edge_map[prev_edge];

        // Find half edge's twin and build connections
        eig::Array2u twin_edge = { curr_edge.y(), curr_edge.x() };
        if (auto it = edge_map.find(twin_edge); it != edge_map.end()) {
          auto &twin_half = m_halfs[it->second];
          curr_half.twin_i = it->second;
          twin_half.twin_i = curr_i;
        }
      }

      // Assign first half-edge to each face
      m_faces[face_i].half_i = edge_map[face[0]];
    }

    // Assign arbitrary half-edge to each vertex
    for (uint i = 0; i < m_halfs.size(); ++i) {
      auto &half = m_halfs[i];
      auto &vert = m_verts[half.vert_i];
      guard_continue(vert.half_i == m_halfs.size());
      vert.half_i = i;
    }
  }

  template <typename T>
  std::vector<uint> HalfEdgeMesh<T>::halfs_storing_vert(uint vert_i) const {
    std::vector<uint> halfs;
    for (uint i = 0; i < m_halfs.size(); ++i) {
      guard_continue(m_halfs[i].vert_i == vert_i);
      halfs.push_back(i);
    }
    return halfs;
  }

  template <typename T>
  std::vector<uint> HalfEdgeMesh<T>::verts_around_face(uint face_i) const {
    std::vector<uint> vertices;
    for (uint half_i : halfs_around_face(face_i)) {
      vertices.push_back(m_halfs[half_i].vert_i);
    }
    return vertices;
  }

  template <typename T>
  std::vector<uint> HalfEdgeMesh<T>::halfs_around_face(uint face_i) const {
    const auto &face = m_faces[face_i];
    const auto &half = m_halfs[face.half_i];
    return { face.half_i, half.next_i, half.prev_i };
  }

  template <typename T>
  std::vector<uint> HalfEdgeMesh<T>::faces_around_vert(uint vert_i) const {
    std::vector<uint> faces;
    const auto &vert = m_verts[vert_i];
    uint half_i = vert.half_i;
    do {
      const auto &half = m_halfs[half_i];
      faces.push_back(half.face_i);
      half_i = m_halfs[half.prev_i].twin_i;
    } while (half_i != vert.half_i);
    return faces;
  }

  template <typename T>
  std::vector<uint> HalfEdgeMesh<T>::faces_around_face(uint face_i) const {
    std::vector<uint> faces;
    const auto &face = m_faces[face_i];
    for (uint half_i : halfs_around_face(face_i)) {
      const auto &half = m_halfs[half_i];
      const auto &twin = m_halfs[half.twin_i];
      faces.push_back(twin.face_i);
    }
    return faces;
  }
  
  template <typename T>
  std::vector<uint> HalfEdgeMesh<T>::faces_around_half(uint half_i) const {
    const Half &half = m_halfs[half_i];
    const Half &twin = m_halfs[half.twin_i];
    return { half.face_i, twin.face_i };
  }

  template <typename T>
  IndexedMesh<T, eig::Array3u> generate_unit_sphere(uint n_subdivs) {
    met_trace();

    using Vt = IndexedMesh<T, eig::Array3u>::Vert;
    using El = IndexedMesh<T, eig::Array3u>::Elem;
    using VMap  = std::unordered_map<Vt, uint, 
                                     decltype(detail::matrix_hash<float>), 
                                     decltype(detail::matrix_equal)>;
    
    // Initial mesh describes an octahedron
    std::vector<Vt> vts = { Vt(-1.f, 0.f, 0.f ), Vt( 0.f,-1.f, 0.f ), Vt( 0.f, 0.f,-1.f ),
                            Vt( 1.f, 0.f, 0.f ), Vt( 0.f, 1.f, 0.f ), Vt( 0.f, 0.f, 1.f ) };
    std::vector<El> els = { El(0, 1, 2), El(3, 2, 1), El(0, 5, 1), El(3, 1, 5),
                            El(0, 4, 5), El(3, 5, 4), El(0, 2, 4), El(3, 4, 2) };

    // Perform loop subdivision several times
    for (uint d = 0; d < n_subdivs; ++d) {        
      std::vector<El> els_(4 * els.size()); // New elements are inserted in this larger vector
      VMap vmap(64, detail::matrix_hash<float>, detail::matrix_equal); // Identical vertices are first tested in this map

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
  IndexedMesh<T, eig::Array3u> generate_convex_hull(const IndexedMesh<T, eig::Array3u> &sphere_mesh,
                                                    std::span<const T> points) {
    met_trace();

    IndexedMesh<T, eig::Array3u> mesh = sphere_mesh;

    fmt::print("Input mesh\n");
    fmt::print("{} verts, {} elems\n", mesh.verts().size(), mesh.elems().size());    
    
    // For each vertex in mesh, each defining a line through the origin:
    std::for_each(std::execution::par_unseq, range_iter(mesh.verts()), [&](auto &v) {
      // Obtain a range of point projections along this line
      auto proj_funct  = [&v](const auto &p) { return v.matrix().dot(p.matrix()); };
      auto proj_range = points | std::views::transform(proj_funct);

      // Find iterator to endpoint, given projections
      auto it = std::ranges::max_element(proj_range);

      // Replace mesh vertex with this endpoint
      v = *(points.begin() + std::distance(proj_range.begin(), it));
    });

    // Data structures for cleanup
    using vertex_ui_map = std::unordered_map<T, uint, 
      decltype(detail::matrix_hash<float>), decltype(detail::matrix_equal)>;
    vertex_ui_map            vert_identify_map;
    std::vector<uint>        vert_remove_list;
    std::unordered_set<uint> vert_removed_list;

    // Structures to flag vertices/elements for removal
    vertex_ui_map     vert_index_map;
    std::vector<bool> vert_remove_flags(mesh.verts().size(), false);
    std::vector<bool> elem_remove_flags(mesh.elems().size(), false);

    // Identify removable double vertices, and update element indices to avoid these
    for (uint i = 0; i < mesh.elems().size(); ++i) {
      auto &elem = mesh.elems()[i];
      for (uint j = 0; j < 3; ++j) {
        auto &indx = elem[j];
        auto &vert = mesh.verts()[indx];

        // If the vert_index_map has a registered vertex already, it is a double
        if (auto it = vert_index_map.find(vert); it != vert_index_map.end()) {
          guard_continue(indx != it->second);
          vert_remove_flags[indx] = true; // Double vertex; flag for removal
          indx = it->second;              // and update referring index
        } else {
          vert_index_map.emplace(vert, indx);
        }
      }
    }

    // Identify removable collapsed elements
    for (uint i = 0; i < mesh.elems().size(); ++i) {
      auto &elem = mesh.elems()[i];
      if (elem[0] == elem[1] || elem[1] == elem[2] || elem[2] == elem[0]) {
        elem_remove_flags[i] = true;
      }
    }

    // Perform an exclusive scan of removal flags to build a new set of indices
    std::vector<uint> vert_new_indices(mesh.verts().size());
    constexpr auto bool_to_ui = [](bool b) { return b ? 0 : 1; };
    std::transform_exclusive_scan(std::execution::par_unseq, 
                                  range_iter(vert_remove_flags),
                                  vert_new_indices.begin(), 0,
                                  std::plus<uint>(), bool_to_ui);

    // Apply new indices to element set
    for (uint i = 0; i < mesh.elems().size(); ++i) {
      auto &elem = mesh.elems()[i];
      for (uint j = 0; j < 3; ++j) {
        elem[j] = vert_new_indices[elem[j]];
      }
    }

    // Obtain indices of erasable vertices and elements from data
    std::vector<uint> vert_remove_indices, elem_remove_indices;
    for (uint i = 0; i < vert_remove_flags.size(); ++i) { 
      if (vert_remove_flags[i]) 
        vert_remove_indices.push_back(i);
    }
    for (uint i = 0; i < elem_remove_flags.size(); ++i) { 
      if (elem_remove_flags[i]) 
        elem_remove_indices.push_back(i);
    }

    // Sort in reverse order to facilitate erasure without modifying iterators
    std::sort(std::execution::par_unseq, range_iter(vert_remove_indices), [](uint i, uint j) { return i > j; });
    std::sort(std::execution::par_unseq, range_iter(elem_remove_indices), [](uint i, uint j) { return i > j; });

    // fmt::print("{}\n", vert_new_indices);
    // fmt::print("{}\n", elem_remove_indices);

    for (uint i : vert_remove_indices)
      mesh.verts().erase(mesh.verts().begin() + i);
    for (uint i : elem_remove_indices)
      mesh.elems().erase(mesh.elems().begin() + i);

    fmt::print("Erased vertices and elements\n");
    fmt::print("{} verts, {} elems\n", mesh.verts().size(), mesh.elems().size());

    /* // Identify identical vertices
    for (auto &elem : mesh.elems()) {
      for (uint &i : elem) {
        const auto &v = mesh.verts()[i];
        if (auto it = vert_identify_map.find(v); it != vert_identify_map.end()) {
          guard_continue(it->second != i);

          // Register vertex for erasure as it is not unique
          vert_remove_list.emplace_back(i);

          // Point element to other already registered vertex
          i = it->second;
        } else {
          // Register vertex as first of its kind
          vert_identify_map.emplace(v, i);
        }
      }
    } */

    /* // Erase identical vertices
    for (auto it = vert_remove_list.begin(); it != vert_remove_list.end(); ++it) {
      uint i = *it;

      // Check that a vertex is not removed twice
      guard_continue(!vert_removed_list.contains(i));
      vert_removed_list.insert(i);

      fmt::print("Erasing {}\n", i);
      mesh.verts().erase(mesh.verts().begin() + i);

      // Update index offsets as an element was removed in the list
      std::for_each(std::execution::par_unseq, range_iter(mesh.elems()),
        [&](auto &elem) { 
          if (elem[0] > i) elem[0]--;
          if (elem[1] > i) elem[1]--;
          if (elem[2] > i) elem[2]--;
      });
      std::for_each(std::execution::par_unseq, range_iter(vert_remove_list),
        [&](uint &j) { 
          guard(j > i);
          j--;
      });
    } */

    /* fmt::print("Erased identical vertices\n");
    fmt::print("{} verts, {} elems\n", mesh.verts().size(), mesh.elems().size()); */

    /* // Erase collapsed triangles
    std::erase_if(mesh.elems(), [&](const auto &e) {
      const uint i = e.x(), j = e.y(), k = e.z();
      return (i == j) || (j == k) || (k == i);
      // return (mesh.verts()[i].isApprox(mesh.verts()[j])) ||
      //        (mesh.verts()[j].isApprox(mesh.verts()[k])) ||
      //        (mesh.verts()[k].isApprox(mesh.verts()[i]));
    }); */

    /* fmt::print("Erased collapsed elements\n");
    fmt::print("{} verts, {} elems\n", mesh.verts().size(), mesh.elems().size()); */

    return mesh;
  }
  
  template <typename T>
  IndexedMesh<T, eig::Array3u> generate_convex_hull(std::span<const T> points) {
    met_trace();
    return generate_convex_hull<T>(generate_unit_sphere<T>(), points);
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

  template <typename T>
  HalfEdgeMesh<T> simplify_mesh(const HalfEdgeMesh<T> &mesh, uint max_vertices) {
    met_trace();
    
    using Vert = HalfEdgeMesh<T>::Vert;
    using Face = HalfEdgeMesh<T>::Face;
    using Half = HalfEdgeMesh<T>::Half;
    
    HalfEdgeMesh<T> new_mesh = mesh;

    for (uint i = 0; i < new_mesh.halfs().size(); ++i) {
      auto &half = new_mesh.halfs()[i];
      auto &twin = new_mesh.halfs()[half.twin_i];
      fmt::print("Half: {} : {} -> {}\n", i, half.vert_i, twin.vert_i);
    }

    fmt::print("\n");
    
    while (new_mesh.verts().size() > max_vertices) {
      // Find half-edge to collapse based on criterion
      // TODO for now, collapse shortest edges
      auto half_it = std::min_element(std::execution::par_unseq, range_iter(new_mesh.halfs()), 
        [&](const Half &a, const Half &b){
          const Half &a_ = new_mesh.halfs()[a.next_i], &b_ = new_mesh.halfs()[b.next_i];
          float a_len = (new_mesh.verts()[a.vert_i].p - new_mesh.verts()[a_.vert_i].p).matrix().norm();
          float b_len = (new_mesh.verts()[b.vert_i].p - new_mesh.verts()[b_.vert_i].p).matrix().norm();
          return a_len < b_len;
        });
      uint half_i = std::distance(new_mesh.halfs().begin(), half_it);

      // Obtain half edges and their vertices, as temporary copy
      Half half = new_mesh.halfs()[half_i], twin = new_mesh.halfs()[half.twin_i];
      Vert &half_vert = new_mesh.verts()[half.vert_i], &twin_vert = new_mesh.verts()[twin.vert_i];

      // Modify vertex position; use the average for now
      half_vert.p = (0.5f * (half_vert.p + twin_vert.p)).eval();

      fmt::print("Shortest half: {} : {} -> {}\n", half_i, half.vert_i, twin.vert_i);

      // Move all references from the second vertex towards the first
      for (auto &other_half : new_mesh.halfs_storing_vert(twin.vert_i))
        new_mesh.halfs()[other_half].vert_i = half.vert_i;

      // Move twins in surrounding faces to stitch halves together, collapsing edge
      Half &ri_next = new_mesh.halfs()[new_mesh.halfs()[half.next_i].twin_i],
           &ri_prev = new_mesh.halfs()[new_mesh.halfs()[half.prev_i].twin_i];
      Half &le_next = new_mesh.halfs()[new_mesh.halfs()[twin.next_i].twin_i],
           &le_prev = new_mesh.halfs()[new_mesh.halfs()[twin.prev_i].twin_i];
      ri_next.twin_i = new_mesh.halfs()[half.prev_i].twin_i;
      ri_prev.twin_i = new_mesh.halfs()[half.next_i].twin_i;
      le_next.twin_i = new_mesh.halfs()[twin.prev_i].twin_i;
      le_prev.twin_i = new_mesh.halfs()[twin.next_i].twin_i;

      // Erase faces and half edges
      std::vector<uint> faces_to_remv = new_mesh.faces_around_half(half_i);
      for (auto face_i : faces_to_remv) {
        // Erase half edges first
        auto halfs_to_remv = new_mesh.halfs_around_face(face_i);
        for (uint i = 0; i < halfs_to_remv.size(); ++i) {
          uint half_j = halfs_to_remv[i];

          // Move vertex to another edge if this is an issue
          auto &vert = new_mesh.verts()[new_mesh.halfs()[half_j].vert_i];
          if (vert.half_i == half_j)
            vert.half_i = new_mesh.halfs()[half_j].next_i;

          // Erase half edge from record
          detail::half_mesh_erase_half(new_mesh, half_j);
          
          // Update record currently in iteration
          std::for_each(range_iter(halfs_to_remv), 
            [&](uint &i) { guard(i > half_j); i--; });
        }

        // Erase face next and update records
        detail::half_mesh_erase_face(new_mesh, face_i);
      }
      
      // Remove collapsed vertex and update records
      detail::half_mesh_erase_vert(new_mesh, twin.vert_i);
    }

    return new_mesh;
  }

  /* Explicit template instantiations for common types */

  template class IndexedMesh<eig::Array3f, eig::Array3u>;
  template class IndexedMesh<eig::AlArray3f, eig::Array3u>;
  template class HalfEdgeMesh<eig::Array3f>;
  template class HalfEdgeMesh<eig::AlArray3f>;

  template IndexedMesh<eig::Array3f, eig::Array3u>   generate_unit_sphere<eig::Array3f>(uint);
  template IndexedMesh<eig::AlArray3f, eig::Array3u> generate_unit_sphere<eig::AlArray3f>(uint);

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

  template IndexedMesh<eig::Array3f, eig::Array2u>
  generate_wireframe<eig::Array3f>(const IndexedMesh<eig::Array3f, eig::Array3u> &);
  template IndexedMesh<eig::AlArray3f, eig::Array2u>
  generate_wireframe<eig::AlArray3f>(const IndexedMesh<eig::AlArray3f, eig::Array3u> &);

  template HalfEdgeMesh<eig::Array3f> 
  simplify_mesh<eig::Array3f>(const HalfEdgeMesh<eig::Array3f> &, uint);
  template HalfEdgeMesh<eig::AlArray3f> 
  simplify_mesh<eig::AlArray3f>(const HalfEdgeMesh<eig::AlArray3f> &, uint);
} // namespace met
