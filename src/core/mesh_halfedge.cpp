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
      std::array<Edge, 3> face = { Edge { el[0], el[1] }, 
                                   Edge { el[1], el[2] }, 
                                   Edge { el[2], el[0] }};
      
      // For each edge in element
      for (Edge &edge : face) {
        // Create initial half-edge and check uniqueness
        m_halfs.push_back(Half { .vert_i = edge.x(), .face_i = face_i });
        debug::check_expr_rel(!edge_map.contains(edge), fmt::format("Edge {} already exists", edge));

        // Register half-edge in connection map
        const uint half_i = m_halfs.size() - 1;
        edge_map.insert({edge, half_i });
      }

      // For each edge in element
      for (uint i = 0; i < face.size(); ++i) {
        // Obtain trio of separated edges in the correct order
        const uint j = (i + 1) % face.size(), k = (j + 1) % face.size();
        auto &edge = face[i], &next = face[j], &prev = face[k];

        // Obtain indices to respective half-edges from edge map
        uint edge_i = edge_map[edge], next_i = edge_map[next], prev_i = edge_map[prev];

        // Fill in half-edge data
        Half &half = m_halfs[edge_i];
        half.next_i = next_i;
        half.prev_i = prev_i;
        
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

    uint vertex_target = mesh.verts().size() - max_vertices;
    while (vertex_target-- >= 0) {
      // Compute half-edge lengths
      std::vector<float> lengths(mesh.halfs().size());
      std::transform(std::execution::par_unseq, range_iter(mesh.halfs()), lengths.begin(),
      [&](const Half &ha) {
        const Half &a_ = mesh.halfs()[a.next_i];
        return (mesh.verts()[a.vert_i].p - mesh.verts()[a_.vert_i].p).matrix().norm();
      });

      // Set lengths for (deleted) halfs to 0
      for (uint i = 0; i < mesh.halfs().size(); ++i)
        if (half_flag_erase[i])
          lengths[i] = std::numeric_limits<float>::min();

      // Find longest half-edge
      uint half_i = std::distance(lengths.begin(), std::min_element(std::execution::par_unseq, range_iter(lengths)));

      // Obtain half edge and twin, as well as their vertices
      Half &half      = mesh.halfs()[half_i], 
           &twin      = mesh.halfs()[half.twin_i];
      Vert &half_vert = mesh.verts()[half.vert_i], 
           &twin_vert = mesh.verts()[twin.vert_i];

      
    }

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