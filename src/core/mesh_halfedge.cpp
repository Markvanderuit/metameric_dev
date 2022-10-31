#include <metameric/core/mesh.hpp>
#include <metameric/core/linprog.hpp>
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

    struct RealizedTriangle {
      eig::Vector3f p0, p1, p2;
      eig::Vector3f n;

      RealizedTriangle(const eig::Vector3f &_p0, const eig::Vector3f &_p1, const eig::Vector3f &_p2)
      : p0(_p0), p1(_p1), p2(_p2),
        n(-(p1 - p0).cross(p2 - p1).normalized().eval()) { }
    };

    eig::Vector3f solve_for_vertex(const std::vector<RealizedTriangle> &triangles,
                                   const eig::Vector3f &min_v = 0.f,
                                   const eig::Vector3f &max_v = 1.f) {
      met_trace();

      constexpr uint N = 3;
      const     uint M = triangles.size();

      // Instantiate constraints matrices
      eig::MatrixXf       A(M, N);
      eig::ArrayXf        b(M);
      eig::ArrayX<LPComp> r(M);

      // Fill constraints data
      for (uint i = 0; i < M; ++i) {
        eig::Vector<float, N> n = (triangles[i].n).eval();
        A.row(i) = n;
        b[i] = n.dot(triangles[i].p0);
        r[i] = LPComp::eEQ;
      }
      
      // Set up other components
      eig::Array<float, N, 1> C = 1.f, l = min_v, u = max_v;

      // Set up parameter object and run minimization
      LPParamsX<float> lp_params { .N = N, .M = M, .C = C, .A = A, .b = b, 
                                   .c0 = 0.f, .r = r, .l = l, .u = u };
      return linprog<float>(lp_params);
    }

    template <typename T>
    std::vector<uint> masked_halfs_storing_vert(const HalfEdgeMesh<T>   &mesh,
                                                const std::vector<bool> &half_mask,
                                                uint vert_i) {
      std::vector<uint> halfs;
      for (uint i = 0; i < half_mask.size(); ++i)
        if (!half_mask[i] && mesh.halfs()[i].vert_i == vert_i)
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

    template <typename T>
    std::vector<uint> masked_faces_around_vert(const HalfEdgeMesh<T>   &mesh,
                                               const std::vector<bool> &half_mask,
                                               uint vert_i) {
      std::vector<uint> faces;
      for (auto h : masked_halfs_storing_vert(mesh, half_mask, vert_i))
        faces.push_back(mesh.halfs()[h].face_i);      
      return faces;
    }

    template <typename T>
    std::unordered_set<uint> masked_faces_around_half(const HalfEdgeMesh<T>   &mesh,
                                                      const std::vector<bool> &half_mask,
                                                      uint half_i) {
      std::unordered_set<uint> face_set;
      const auto &hl = mesh.halfs()[half_i];
      for (const auto &h : { hl, mesh.halfs()[hl.twin_i] })
        std::ranges::copy(masked_faces_around_vert(mesh, half_mask, h.vert_i),
          std::inserter(face_set, face_set.begin()));
      return face_set;
    }
   } // namespace detail

  template <typename T>
  HalfEdgeMesh<T>::HalfEdgeMesh(const IndexedMesh<T, eig::Array3u> other) {
    using Edge = eig::Array2u;
    using Emap = std::unordered_map<Edge, uint, detail::eig_hash_t<uint>, detail::eig_equal_t>;

    // Allocate known record space and temporary containers
    m_verts.resize(other.verts().size());
    m_faces.resize(other.elems().size());
    m_halfs.reserve(m_faces.size() * 3);
    Emap edge_map(16, detail::eig_hash<uint>, detail::eig_equal);

    // Initialize vertex positions
    const uint vertex_to_half_i = static_cast<uint>(m_faces.size()) * 3; // Initial id stored in verts
    std::transform(std::execution::par_unseq, range_iter(other.verts()), m_verts.begin(), 
      [&](const auto &p) { return Vert { p, vertex_to_half_i }; });
    
    // Process triangle elements into half-edges
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
    
    // Temporary containers used during minimization
    std::vector<bool>  vert_flag_rem(mesh.verts().size(), false);
    std::vector<bool>  face_flag_rem(mesh.faces().size(), false);
    std::vector<bool>  half_flag_rem(mesh.halfs().size(), false);
    std::vector<float> collapse_metr(mesh.halfs().size(), 0.f);
    std::vector<T>     collapse_vert(mesh.halfs().size(), T(0.f));

    // Keep minimizing until maximum vertex count is satisfied
    uint curr_vertices = mesh.verts().size();
    while (curr_vertices-- > max_vertices) {
      // Compute half-edge collapse metric
      #pragma omp parallel for
      for (int i = 0; i < collapse_metr.size(); ++i) {
        // Half-edge has already been collapsed and marked for removal
        guard_continue(!half_flag_rem[i]);

        // Obtain const half edge and its twin
        const Half &half   = mesh.halfs()[i],            
                   &twin   = mesh.halfs()[half.twin_i];
        const Vert &half_v = mesh.verts()[half.vert_i], 
                   &twin_v = mesh.verts()[twin.vert_i];

        // Build set of connected vertices
        auto half_nbors = detail::masked_verts_around_vert(mesh, half_flag_rem, half.vert_i);
        auto twin_nbors = detail::masked_verts_around_vert(mesh, half_flag_rem, twin.vert_i);
        std::ranges::sort(half_nbors);
        std::ranges::sort(twin_nbors);
        std::vector<uint> connected_verts;
        connected_verts.reserve(half_nbors.size() + twin_nbors.size());
        std::ranges::set_intersection(half_nbors, twin_nbors, std::back_inserter(connected_verts));

        // Test for connectivity to avoid non-manifold collapses
        if (connected_verts.size() > 2) {
          collapse_metr[i] = std::numeric_limits<float>::max();
          continue;
        }
        
        // Test if vertex positions are identical, in which case collapse is essentially free
        if (half_v.p.isApprox(twin_v.p)) {
          collapse_metr[i] = 0.f;
          collapse_vert[i] = half_v.p;
          continue;
        }
        
        // Given non-equal vertices, solve for new optimal vertex position 
        // based on planes formed by neighboring triangles
        std::vector<detail::RealizedTriangle> tris;
        for (auto &face_i : detail::masked_faces_around_half(mesh, half_flag_rem, i)) {
          auto v = mesh.verts_around_face(face_i);
          tris.push_back(detail::RealizedTriangle(mesh.verts()[v[0]].p, mesh.verts()[v[1]].p, mesh.verts()[v[2]].p));
        }
        auto avg_p = 0.5f * (half_v.p + twin_v.p);
        auto new_p = detail::solve_for_vertex(tris, (avg_p - 0.1f).max(0.f).eval(), (avg_p + 0.1f).min(1.f).eval());
        
        // Compute resulting cost metric of solved-for vertex
        float metric = 0.f;
        for (const auto &tri : tris)
          metric += std::abs(tri.n.dot(new_p - tri.p0));

        collapse_metr[i] = metric;
        collapse_vert[i] = new_p;
      }

      // Find cheapest collapsible half-edge satisfying all criteria
      uint half_i = std::distance(collapse_metr.begin(), 
        std::min_element(std::execution::par_unseq, range_iter(collapse_metr)));

      // Obtain half edge and its twin
      Half &half = mesh.halfs()[half_i], 
           &twin = mesh.halfs()[half.twin_i];
           
      // Throw on a debug error resulting in non-manifold meshes
      debug::check_expr_dbg(half.vert_i != twin.vert_i, 
        "Error while simplifying mesh; non-manifold geometry detected");
        
      // Shift this edge's vertex position to the solved for position
      mesh.verts()[half.vert_i].p = collapse_vert[half_i];

      // Move all uses of the twin vertex to this edge's vertex, but retain id (as it'll be affected)
      const uint twin_vert_i = twin.vert_i;
      for (uint h : detail::masked_halfs_storing_vert(mesh, half_flag_rem, twin_vert_i))
        mesh.halfs()[h].vert_i = half.vert_i;
      
      // Connext twins of next/prev halfs to stitch together their edges
      auto &half_nx = mesh.halfs()[half.next_i], 
           &half_pr = mesh.halfs()[half.prev_i],
           &twin_nx = mesh.halfs()[twin.next_i], 
           &twin_pr = mesh.halfs()[twin.prev_i];
      mesh.halfs()[half_nx.twin_i].twin_i = half_pr.twin_i;      
      mesh.halfs()[half_pr.twin_i].twin_i = half_nx.twin_i;   
      mesh.halfs()[twin_nx.twin_i].twin_i = twin_pr.twin_i;      
      mesh.halfs()[twin_pr.twin_i].twin_i = twin_nx.twin_i;
      
      // Flag affected vertices/faces components for erasure
      vert_flag_rem[twin_vert_i] = true;
      face_flag_rem[half.face_i] = true;
      face_flag_rem[twin.face_i] = true;
      for (uint h : mesh.halfs_around_face(half.face_i)) {
        half_flag_rem[h] = true;
        collapse_metr[h] = std::numeric_limits<float>::max();
      }
      for (uint h : mesh.halfs_around_face(twin.face_i)) {
        half_flag_rem[h] = true;
        collapse_metr[h] = std::numeric_limits<float>::max();
      }
    }

    // Determine new indices of non-deleted verts, faces, and halfs through exclusive prefix scan
    std::vector<uint> vert_idx_new(mesh.verts().size());
    std::vector<uint> face_idx_new(mesh.faces().size());
    std::vector<uint> half_idx_new(mesh.halfs().size());
    std::transform_inclusive_scan(std::execution::par_unseq, range_iter(vert_flag_rem),
      vert_idx_new.begin(), std::plus<uint>(), [](bool b) { return b ? 0 : 1; });
    std::transform_inclusive_scan(std::execution::par_unseq, range_iter(face_flag_rem),
      face_idx_new.begin(), std::plus<uint>(), [](bool b) { return b ? 0 : 1; });
    std::transform_inclusive_scan(std::execution::par_unseq, range_iter(half_flag_rem),
      half_idx_new.begin(), std::plus<uint>(), [](bool b) { return b ? 0 : 1; });
    std::for_each(std::execution::par_unseq, range_iter(vert_idx_new), [](uint &i) { i--; });
    std::for_each(std::execution::par_unseq, range_iter(face_idx_new), [](uint &i) { i--; });
    std::for_each(std::execution::par_unseq, range_iter(half_idx_new), [](uint &i) { i--; });

    // Apply new indices to verts, faces, and halfs
    std::for_each(std::execution::par_unseq, range_iter(mesh.verts()), 
      [&](Vert &v) { v.half_i = half_idx_new[v.half_i]; });
    std::for_each(std::execution::par_unseq, range_iter(mesh.faces()), 
      [&](Face &f) { f.half_i = half_idx_new[f.half_i]; });
    std::for_each(std::execution::par_unseq, range_iter(mesh.halfs()), [&](Half &h) {
      h.twin_i = half_idx_new[h.twin_i];
      h.next_i = half_idx_new[h.next_i];
      h.prev_i = half_idx_new[h.prev_i];
      h.vert_i = vert_idx_new[h.vert_i];
      h.face_i = face_idx_new[h.face_i];
    });

    // Erase marked verts, faces and halfs
    std::erase_if(mesh.verts(), [&](const Vert &v) { return vert_flag_rem[&v - &*mesh.verts().begin()]; });
    std::erase_if(mesh.faces(), [&](const Face &v) { return face_flag_rem[&v - &*mesh.faces().begin()]; });
    std::erase_if(mesh.halfs(), [&](const Half &v) { return half_flag_rem[&v - &*mesh.halfs().begin()]; });
    
    return mesh;
  }

  template <typename T>
  IndexedMesh<T, eig::Array3u> simplify_mesh(const IndexedMesh<T, eig::Array3u> &mesh, uint max_vertices) {
    return simplify_mesh(HalfEdgeMesh<T>(mesh), max_vertices);
  }

  /* Explicit template instantiations for common types */

  template class HalfEdgeMesh<eig::Array3f>;
  template class HalfEdgeMesh<eig::AlArray3f>;

  template HalfEdgeMesh<eig::Array3f> 
  simplify_mesh<eig::Array3f>(const HalfEdgeMesh<eig::Array3f> &, uint);
  template HalfEdgeMesh<eig::AlArray3f> 
  simplify_mesh<eig::AlArray3f>(const HalfEdgeMesh<eig::AlArray3f> &, uint);
  template IndexedMesh<eig::Array3f, eig::Array3u> 
  simplify_mesh<eig::Array3f>(const IndexedMesh<eig::Array3f, eig::Array3u> &, uint);
  template IndexedMesh<eig::AlArray3f, eig::Array3u> 
  simplify_mesh<eig::AlArray3f>(const IndexedMesh<eig::AlArray3f, eig::Array3u> &, uint);
} // namespace met