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
  } // namespace detail

  template <typename T>
  IndexedMesh<T>::IndexedMesh(std::span<const Vert> verts, std::span<const Elem> elems)
  : m_verts(range_iter(verts)), m_elems(range_iter(elems)) { }

  template <typename T>
  IndexedMesh<T>::IndexedMesh(const HalfEdgeMesh<T> &other) {
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
      m_elems[i] = Elem { verts[0], verts[1], verts[2] };
    }
  }

  template <typename T>
  HalfEdgeMesh<T>::HalfEdgeMesh(const IndexedMesh<T> other) {
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
  IndexedMesh<T> generate_unit_sphere(uint n_subdivs) {
    met_trace();

    using Vt = IndexedMesh<T>::Vert;
    using El = IndexedMesh<T>::Elem;
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
  IndexedMesh<T> generate_convex_hull(const IndexedMesh<T> &sphere_mesh,
                                      std::span<const T> points) {
    met_trace();

    IndexedMesh<T> mesh = sphere_mesh;
    HalfEdgeMesh<T> mesh_(mesh);

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
  
  template <typename T>
  IndexedMesh<T> generate_convex_hull(std::span<const T> points) {
    met_trace();
    return generate_convex_hull<T>(generate_unit_sphere<T>(), points);
  }

  template <typename T>
  HalfEdgeMesh<T> simplify_mesh(const HalfEdgeMesh<T> &mesh, uint max_vertices) {
    using Vert = HalfEdgeMesh<T>::Vert;
    using Face = HalfEdgeMesh<T>::Face;
    using Half = HalfEdgeMesh<T>::Half;
    
    HalfEdgeMesh<T> new_mesh = mesh;
    
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
      half_vert.p = 0.5f * (half_vert.p + twin_vert.p);

      // Move references from the second vertex towards the first
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
          // Erase half edge from record
          uint half_i = halfs_to_remv[i];
          new_mesh.halfs().erase(new_mesh.halfs().begin() + half_i);

          // Update rest of records
          std::for_each(std::execution::par_unseq, range_iter(new_mesh.verts()),
            [&](Vert &vert) { guard(vert.half_i > half_i); vert.half_i--; });
          std::for_each(std::execution::par_unseq, range_iter(new_mesh.faces()),
            [&](Face &face) { guard(face.half_i > half_i); face.half_i--; });
          std::for_each(std::execution::par_unseq, range_iter(new_mesh.halfs()),
            [&](Half &half) {
              if (half.next_i > half_i) half.next_i--;
              if (half.prev_i > half_i) half.prev_i--;
              if (half.twin_i > half_i) half.twin_i--;
            });
          
          // Update record currently in iteration
          std::for_each(range_iter(halfs_to_remv), [&](uint &i) { guard(i > half_i); i--; });
        }

        // Erase face next and update records
        new_mesh.faces().erase(new_mesh.faces().begin() + face_i);
        std::for_each(std::execution::par_unseq, range_iter(new_mesh.halfs()),
          [&](Half &half) { guard(half.face_i > face_i); half.face_i--; });
      }
      
      // Remove collapsed vertex and update records
      new_mesh.verts().erase(new_mesh.verts().begin() + twin.vert_i);
      std::for_each(std::execution::par_unseq, range_iter(new_mesh.halfs()),
        [&](Half &half) { guard(half.vert_i > twin.vert_i); half.vert_i--; });
    }

    return new_mesh;
  }

  /* Explicit template instantiations for common types */

  template class IndexedMesh<eig::Array3f>;
  template class IndexedMesh<eig::AlArray3f>;
  template class HalfEdgeMesh<eig::Array3f>;
  template class HalfEdgeMesh<eig::AlArray3f>;

  template IndexedMesh<eig::Array3f>
  generate_unit_sphere<eig::Array3f>(uint);
  template IndexedMesh<eig::AlArray3f>
  generate_unit_sphere<eig::AlArray3f>(uint);

  template IndexedMesh<eig::Array3f> 
  generate_convex_hull<eig::Array3f>(const IndexedMesh<eig::Array3f> &sphere_mesh,
                                     std::span<const eig::Array3f>);
  template IndexedMesh<eig::AlArray3f> 
  generate_convex_hull<eig::AlArray3f>(const IndexedMesh<eig::AlArray3f> &sphere_mesh,
                                       std::span<const eig::AlArray3f>);
                                       
  template IndexedMesh<eig::Array3f> 
  generate_convex_hull<eig::Array3f>(std::span<const eig::Array3f>);
  template IndexedMesh<eig::AlArray3f> 
  generate_convex_hull<eig::AlArray3f>(std::span<const eig::AlArray3f>);

  template HalfEdgeMesh<eig::Array3f> 
  simplify_mesh<eig::Array3f>(const HalfEdgeMesh<eig::Array3f> &, uint);
  template HalfEdgeMesh<eig::AlArray3f> 
  simplify_mesh<eig::AlArray3f>(const HalfEdgeMesh<eig::AlArray3f> &, uint);
} // namespace met
