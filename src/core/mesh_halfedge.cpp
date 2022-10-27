#include <metameric/core/mesh.hpp>
#include <metameric/core/utility.hpp>
#include <metameric/core/detail/trace.hpp>
#include <fmt/ranges.h>
#include <algorithm>
#include <execution>
#include <limits>
#include <ranges>
#include <unordered_set>
#include <unordered_map>
#include <vector>

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

    template <typename T>
    std::vector<uint> masked_halfs_storing_vert(const HalfEdgeMesh<T>   &mesh,
                                                const std::vector<bool> &mask,
                                                uint vert_i) {
      std::vector<uint> halfs;
      for (uint i = 0; i < mask.size(); ++i)
        if (!mask[i] && mesh.halfs()[i].vert_i == vert_i)
          halfs.push_back(i);
      return halfs;
    }

    template <typename T>
    std::vector<uint> masked_verts_around_vert(const HalfEdgeMesh<T>   &mesh,
                                               const std::vector<bool> &half_mask,
                                               uint vert_i) {
      std::vector<uint> verts;
      for (uint h : masked_halfs_storing_vert(mesh, half_mask, vert_i))
        verts.push_back(mesh.halfs()[mesh.halfs()[h].twin_i].vert_i);
      return verts;
    }
   } // namespace detail

  template <typename T>
  HalfEdgeMesh<T>::HalfEdgeMesh(const IndexedMesh<T, eig::Array3u> other) {
    // Allocate known record space
    m_verts.resize(other.verts().size());
    m_faces.resize(other.elems().size());
    m_halfs.reserve(m_faces.size() * 3);

    // Initial id stored in each vertex
    const uint vertex_to_half_i = static_cast<uint>(m_faces.size()) * 3;

    // Initialize vertex positions
    std::transform(std::execution::par_unseq, range_iter(other.verts()), m_verts.begin(), 
      [&](const auto &p) { return Vert { p, vertex_to_half_i }; });
    
    // Create map to perform half-edge connections
    using Edge = eig::Array2u;
    using Emap = std::unordered_map<Edge, uint, detail::eig_hash_t<uint>, detail::eig_equal_t>;
    Emap edge_map(16, detail::eig_hash<uint>, detail::eig_equal);
    
    for (auto it = other.elems().begin(); it != other.elems().end(); ++it) {
      // Obtain current element and split into separate edges
      const auto &el = *it;
      const uint face_i = std::distance(other.elems().begin(), it);
      std::array<Edge, 3> face = { 
        Edge { el[0], el[1] }, Edge { el[1], el[2] }, Edge { el[2], el[0] }};
      
      // For each edge in element
      for (Edge &edge : face) {
        // Create initial half-edge and check uniqueness
        m_halfs.push_back(Half { .vert_i = edge[0], .face_i = face_i });
        debug::check_expr_rel(!edge_map.contains(edge), fmt::format("Edge {} already exists", edge));

        // Register half-edge in connection map
        edge_map.insert({edge, static_cast<uint>(m_halfs.size() - 1) });
      }

      // For each edge in element
      for (uint i = 0; i < face.size(); ++i) {
        // Obtain trio of separated edges in the correct order
        const uint j = (i + 1) % face.size(), k = (i + 2) % face.size();
        auto &edge = face[i], &next = face[j], &prev = face[k];

        // Fill in half-edge data
        uint edge_i = edge_map[edge];
        Half &half = m_halfs[edge_i];
        half.next_i = edge_map[next];
        half.prev_i = edge_map[prev];
        
        // Attempt to find twin edge
        if (auto it = edge_map.find(Edge { edge.y(), edge.x() }); it != edge_map.end()) {
          uint twin_i = it->second;
          Half &twin = m_halfs[twin_i];

          // If twin is found, establish connection
          half.twin_i = twin_i;
          twin.twin_i = edge_i;
        }
      }

      // Lastly, refer to first of these three edges from face
      m_faces[face_i].half_i = edge_map[face[0]];
    }

    // Finally, assign arbitrary half-edge to each vertex
    for (uint i = 0; i < m_halfs.size(); ++i) {
      Half &half = m_halfs[i];
      Vert &vert = m_verts[half.vert_i];
      if (vert.half_i == vertex_to_half_i)
        vert.half_i = i;
    }
  }

  template <typename T>
  std::vector<uint> HalfEdgeMesh<T>::verts_around_vert(uint vert_i) const {
    std::vector<uint> verts;
    for (auto &h : halfs_storing_vert(vert_i))
      verts.push_back(m_halfs[m_halfs[h].twin_i].vert_i);
    return verts;
  }

  template <typename T>
  std::vector<uint> HalfEdgeMesh<T>::halfs_storing_vert(uint vert_i) const {
    std::vector<uint> halfs;
    for (uint i = 0; i < m_halfs.size(); ++i)
      if (m_halfs[i].vert_i == vert_i)
        halfs.push_back(i);
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
    const Face &face = m_faces[face_i];
    const Half &half = m_halfs[face.half_i];
    return { face.half_i, half.next_i, half.prev_i };
  }

  template <typename T>
  std::vector<uint> HalfEdgeMesh<T>::faces_around_vert(uint vert_i) const {
    std::vector<uint> faces;
    const Vert &vert = m_verts[vert_i];
    uint half_i = vert.half_i;
    do {
      const Half &half = m_halfs[half_i];
      faces.push_back(half.face_i);
      half_i = m_halfs[half.prev_i].twin_i;
    } while (half_i != vert.half_i);
    return faces;
  }

  template <typename T>
  std::vector<uint> HalfEdgeMesh<T>::faces_around_face(uint face_i) const {
    std::vector<uint> faces;
    const auto &face = m_faces[face_i];
    for (uint half_i : halfs_around_face(face_i))
      faces.push_back(m_halfs[m_halfs[half_i].twin_i].face_i);
    return faces;
  }
  
  template <typename T>
  std::vector<uint> HalfEdgeMesh<T>::faces_around_half(uint half_i) const {
    const Half &half = m_halfs[half_i];
    const Half &twin = m_halfs[half.twin_i];
    return { half.face_i, twin.face_i };
  }

  template <typename T>
  HalfEdgeMesh<T> simplify_mesh(const HalfEdgeMesh<T> &input_mesh, uint max_vertices) {
    met_trace();

    using Vert = HalfEdgeMesh<T>::Vert;
    using Face = HalfEdgeMesh<T>::Face;
    using Half = HalfEdgeMesh<T>::Half;

    HalfEdgeMesh<T> mesh = input_mesh;
    
    std::vector<bool> vert_flag_erase(mesh.verts().size(), false);
    std::vector<bool> face_flag_erase(mesh.faces().size(), false);
    std::vector<bool> half_flag_erase(mesh.halfs().size(), false);

    uint vert_count = mesh.verts().size();
    uint face_count = mesh.faces().size();
    while (vert_count > max_vertices) {
      fmt::print("{} - {}\n", vert_count, face_count);

      // Compute half-edge lengths // TODO: employ different metric
      std::vector<float> lengths(mesh.halfs().size());
      std::transform(std::execution::par_unseq, range_iter(mesh.halfs()), lengths.begin(),
      [&](const Half &a) {
        auto v = (mesh.verts()[a.vert_i].p
               -  mesh.verts()[mesh.halfs()[a.twin_i].vert_i].p).matrix().eval();
        return v.isZero() ? 0.f : v.norm();
      });

      // Set lengths for (deleted) halfs to 0
      for (uint i = 0; i < mesh.halfs().size(); ++i)
        if (half_flag_erase[i])
          lengths[i] = std::numeric_limits<float>::max();

      // Find shortest collapsible half-edge satisfying collapse criteria
      uint half_i;
      while (true) {
        // Find shortest half-edge pair
        half_i = std::distance(lengths.begin(), std::min_element(std::execution::par_unseq, range_iter(lengths)));
        Half &half = mesh.halfs()[half_i], &twin = mesh.halfs()[half.twin_i];

        // Test for correct connectivity
        auto half_neighbours = detail::masked_verts_around_vert(mesh, half_flag_erase, half.vert_i);
        auto twin_neighbours = detail::masked_verts_around_vert(mesh, half_flag_erase, twin.vert_i);
        std::ranges::sort(half_neighbours);
        std::ranges::sort(twin_neighbours);
        std::vector<uint> intersection;
        std::ranges::set_intersection(half_neighbours, twin_neighbours, std::back_inserter(intersection));
        
        if (intersection.size() > 2) {
          lengths[half_i] = std::numeric_limits<float>::max();
          continue;
        }
        fmt::print("Overlap: {}\n", intersection.size());

        break;
      }

      fmt::print("Shortest is {} at {}\n", half_i, lengths[half_i]);

      // Obtain half edge and twin, as well as their respective vertices
      Half &half      = mesh.halfs()[half_i], 
           &twin      = mesh.halfs()[half.twin_i];
      Vert &half_vert = mesh.verts()[half.vert_i], 
           &twin_vert = mesh.verts()[twin.vert_i];

      // Shift vertex positions to their respective average center
      half_vert.p = ((half_vert.p + twin_vert.p) * 0.5f).eval();
      // twin_vert.p = half_vert.p; // TODO: redundant

      // Move all uses of the twin vertex to this edge's vertex, but store the id
      const uint twin_vert_i_copy = twin.vert_i;
      for (uint h : detail::masked_halfs_storing_vert(mesh, half_flag_erase, twin_vert_i_copy))
      // for (uint h : mesh.halfs_storing_vert(twin_vert_i_copy))
        mesh.halfs()[h].vert_i = half.vert_i;
      
      // Obtain the twins of next/prev halfs to right and left
      Half &righ_next = mesh.halfs()[mesh.halfs()[half.next_i].twin_i],
           &righ_prev = mesh.halfs()[mesh.halfs()[half.prev_i].twin_i];
      Half &left_next = mesh.halfs()[mesh.halfs()[twin.next_i].twin_i],
           &left_prev = mesh.halfs()[mesh.halfs()[twin.prev_i].twin_i];

      fmt::print("righ face = {}, half = {} - {} - {}, left face = {}, half = {} - {} - {}\n",
        half.face_i,
        half_i, half.next_i, half.prev_i,
        twin.face_i,
        half.twin_i, twin.next_i, twin.prev_i
      );

      // Connect these through their own twins
      righ_next.twin_i = mesh.halfs()[half.prev_i].twin_i;
      righ_prev.twin_i = mesh.halfs()[half.next_i].twin_i;
      left_next.twin_i = mesh.halfs()[twin.prev_i].twin_i;
      left_prev.twin_i = mesh.halfs()[twin.next_i].twin_i;
      
      if (half.vert_i != twin_vert_i_copy) {
        vert_count--;
        vert_flag_erase[twin_vert_i_copy] = true;
      } else {
        fmt::print("Uhh!\n");
      }

      // Flag components for deletion
      face_flag_erase[half.face_i] = true;
      face_flag_erase[twin.face_i] = true;
      face_count -= 2;
      for (uint h : mesh.halfs_around_face(half.face_i)) half_flag_erase[h] = true;
      for (uint h : mesh.halfs_around_face(twin.face_i)) half_flag_erase[h] = true;
    }

    // Determine new indices of non-deleted vertices, faces, and halfs
    // through exclusive prefix scan
    std::vector<uint> vert_indx_new(mesh.verts().size());
    std::vector<uint> face_indx_new(mesh.faces().size());
    std::vector<uint> half_indx_new(mesh.halfs().size());
    std::transform_inclusive_scan(std::execution::par_unseq, range_iter(vert_flag_erase),
      vert_indx_new.begin(), std::plus<uint>(), [](bool b) { return b ? 0 : 1; });
    std::transform_inclusive_scan(std::execution::par_unseq, range_iter(face_flag_erase),
      face_indx_new.begin(), std::plus<uint>(), [](bool b) { return b ? 0 : 1; });
    std::transform_inclusive_scan(std::execution::par_unseq, range_iter(half_flag_erase),
      half_indx_new.begin(), std::plus<uint>(), [](bool b) { return b ? 0 : 1; });
    std::for_each(std::execution::par_unseq, range_iter(vert_indx_new), [](uint &i) { i--; });
    std::for_each(std::execution::par_unseq, range_iter(face_indx_new), [](uint &i) { i--; });
    std::for_each(std::execution::par_unseq, range_iter(half_indx_new), [](uint &i) { i--; });

    // Apply new indices to verts, faces, and halfs
    for (Vert &v : mesh.verts()) v.half_i = half_indx_new[v.half_i];
    for (Face &f : mesh.faces()) f.half_i = half_indx_new[f.half_i];
    for (Half &h : mesh.halfs()) {
      h.twin_i = half_indx_new[h.twin_i];
      h.next_i = half_indx_new[h.next_i];
      h.prev_i = half_indx_new[h.prev_i];
      h.vert_i = vert_indx_new[h.vert_i];
      h.face_i = face_indx_new[h.face_i];
    }

    // Obtain indices of deleted verts, faces, and halfs in reverse order
    std::vector<uint> vert_indx_erase;
    std::vector<uint> face_indx_erase;
    std::vector<uint> half_indx_erase;
    for (auto it = vert_flag_erase.rbegin(); it != vert_flag_erase.rend(); ++it)
      if (bool flag = *it; flag) vert_indx_erase.push_back(std::distance(it, vert_flag_erase.rend()) - 1);
    for (auto it = face_flag_erase.rbegin(); it != face_flag_erase.rend(); ++it)
      if (bool flag = *it; flag) face_indx_erase.push_back(std::distance(it, face_flag_erase.rend()) - 1);
    for (auto it = half_flag_erase.rbegin(); it != half_flag_erase.rend(); ++it)
      if (bool flag = *it; flag) half_indx_erase.push_back(std::distance(it, half_flag_erase.rend()) - 1);
    
    // Erase marked verts, faces and halfs in reverse order
    for (uint i : vert_indx_erase) mesh.verts().erase(mesh.verts().begin() + i);
    for (uint i : face_indx_erase) mesh.faces().erase(mesh.faces().begin() + i);
    for (uint i : half_indx_erase) mesh.halfs().erase(mesh.halfs().begin() + i);

    return mesh;
  }

 /*  template <typename T>
  HalfEdgeMesh<T> simplify_mesh(const HalfEdgeMesh<T> &mesh, uint max_vertices) {
    met_trace();
    
    using Vert = HalfEdgeMesh<T>::Vert;
    using Face = HalfEdgeMesh<T>::Face;
    using Half = HalfEdgeMesh<T>::Half;
    
    // fmt::print("Beginning halfedge generation\n");
    HalfEdgeMesh<T> new_mesh = mesh;
    for (uint i = 0; i < new_mesh.halfs().size(); ++i) {
      auto &half = new_mesh.halfs()[i];
      auto &twin = new_mesh.halfs()[half.twin_i];
      // fmt::print("Half: {} : {} -> {}\n", i, half.vert_i, twin.vert_i);
    }

    // fmt::print("\n");
    
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
      Half half       = new_mesh.halfs()[half_i], 
           twin       = new_mesh.halfs()[half.twin_i];
      Vert &half_vert = new_mesh.verts()[half.vert_i], 
           &twin_vert = new_mesh.verts()[twin.vert_i];

      // Modify vertex position; use the average for now
      half_vert.p = (0.5f * (half_vert.p + twin_vert.p)).eval();

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
      std::vector<uint> faces_to_remv = { half.face_i, twin.face_i };
      std::ranges::sort(faces_to_remv, [](uint i, uint j) { return i > j; });
      for (auto face_i : faces_to_remv) {
        // Erase half edges first
        auto halfs_to_remv = new_mesh.halfs_around_face(face_i);
        std::ranges::sort(halfs_to_remv, [](uint i, uint j) { return i > j; });
        for (uint i = 0; i < halfs_to_remv.size(); ++i) {
          uint half_j = halfs_to_remv[i];

          // Move vertex to another edge if this is an issue
          auto &vert = new_mesh.verts()[new_mesh.halfs()[half_j].vert_i];
          if (vert.half_i == half_j)
            vert.half_i = new_mesh.halfs()[half_j].next_i;

          // Erase half edge from record
          detail::half_mesh_erase_half(new_mesh, half_j);
        }

        // Erase face next and update records
        detail::half_mesh_erase_face(new_mesh, face_i);
      }
      
      // Remove collapsed vertex and update records
      detail::half_mesh_erase_vert(new_mesh, twin.vert_i);
    }

    return new_mesh;
  } */

  /* Explicit template instantiations for common types */

  template class HalfEdgeMesh<eig::Array3f>;
  template class HalfEdgeMesh<eig::AlArray3f>;

  template HalfEdgeMesh<eig::Array3f> 
  simplify_mesh<eig::Array3f>(const HalfEdgeMesh<eig::Array3f> &, uint);
  template HalfEdgeMesh<eig::AlArray3f> 
  simplify_mesh<eig::AlArray3f>(const HalfEdgeMesh<eig::AlArray3f> &, uint);
} // namespace met