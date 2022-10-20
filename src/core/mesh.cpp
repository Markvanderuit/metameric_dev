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
  HalfEdgeMesh<T>::HalfEdgeMesh(const IndexedMesh<T> other) {
    // Allocate record space
    m_vertices.resize(other.vertices.size());
    m_faces.resize(other.elements.size());

    // Initialize vertex positions
    std::transform(range_iter(other.vertices), m_vertices.begin(), [&](const auto &p) { 
      return Vertex { p, static_cast<uint>(other.vertices.size()) }; });
    
    // Map to perform half-edge connections
    using Edge  = eig::Array2u;
    using Edges = std::unordered_map<Edge, uint, 
                                     decltype(detail::matrix_hash<uint>), 
                                     decltype(detail::matrix_equal)>;
    Edges edge_map;

    for (uint face_i = 0; face_i < other.elements.size(); ++face_i) {
      const auto &el = other.elements[face_i];
      std::array<Edge, 3> face = { Edge { el.x(), el.y() },
                                   Edge { el.y(), el.z() }, 
                                   Edge { el.z(), el.x() }};

      // Insert initial half-edge data for this face 
      for (uint i = 0; i < face.size(); ++i) {
        auto &edge = face[i];
        HalfEdge half = { .vert_i = edge.x(), .face_i = face_i };
        m_halves.push_back(half);
        edge_map.insert({ edge, static_cast<uint>(m_halves.size() - 1) });
      }

      // Establish connections
      for (uint i = 0; i < face.size(); ++i) {
        // Find next/previous edges in face: j = next, k = prev
        const uint j = (i + 1) % face.size(), k = (j + 1) % face.size();
        auto &curr_edge = face[i], &next_edge = face[j], &prev_edge = face[k];

        // Find the current half edge and assign connections
        auto &curr_half  = m_halves[edge_map[curr_edge]];
        curr_half.next_i = edge_map[next_edge];
        curr_half.prev_i = edge_map[prev_edge];

        // Find half edge's twin and assign connections
        eig::Array2u twin_edge = { curr_edge.y(), curr_edge.x() };
        if (auto it = edge_map.find(twin_edge); it != edge_map.end()) {
          auto &twin_half = m_halves[it->second];
          curr_half.twin_i = it->second;
          twin_half.twin_i = edge_map[curr_edge];
        }
      }

      // Assign first half-edge to each face
      m_faces[face_i].half_i = edge_map[face[0]];
    }

    // Assign arbitrary half-edge to each vertex
    for (uint i = 0; i < m_halves.size(); ++i) {
      auto &half = m_halves[i];
      auto &vert = m_vertices[half.vert_i];
      guard_continue(vert.half_i == other.vertices.size());
      vert.half_i = i;
    }
  }

  template <typename T>
  std::vector<uint> HalfEdgeMesh<T>::vertices_for_face(uint face_i) {
    std::vector<uint> vertices;
    for (uint half_i : halves_for_face(face_i)) {
      vertices.push_back(m_halves[half_i].vert_i);
    }
    return vertices;
  }


  template <typename T>
  std::vector<uint> HalfEdgeMesh<T>::halves_for_face(uint face_i) {
    const auto &face = m_faces[face_i];
    const auto &half = m_halves[face.half_i];
    return { face.half_i, half.next_i, half.prev_i };
  }

  template <typename T>
  std::vector<uint> HalfEdgeMesh<T>::faces_around_vertex(uint vert_i) {
    std::vector<uint> faces;
    const auto &vert = m_vertices[vert_i];
    uint half_i = vert.half_i;
    do {
      const auto &half = m_halves[half_i];
      faces.push_back(half.face_i);
      half_i = m_halves[half.prev_i].twin_i;
    } while (half_i != vert.half_i);
    return faces;
  }

  template <typename T>
  std::vector<uint> HalfEdgeMesh<T>::faces_around_face(uint face_i) {
    std::vector<uint> faces;
    const auto &face = m_faces[face_i];
    for (uint half_i : halves_for_face(face_i)) {
      const auto &half = m_halves[half_i];
      const auto &twin = m_halves[half.twin_i];
      faces.push_back(twin.face_i);
    }
    return faces;
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
  
  template <typename T>
  IndexedMesh<T> generate_convex_hull(std::span<const T> points) {
    met_trace();
    return generate_convex_hull<T>(generate_unit_sphere<T>(), points);
  }

  template <typename T>
  IndexedMesh<T> simplify_mesh(const IndexedMesh<T> &mesh, uint max_vertices) {
    IndexedMesh<T> new_mesh = mesh;
    
    while (new_mesh.vertices.size() > max_vertices) {
      // Find mesh to collapse based on criterion

      // Identify faces that need modification

      // Rebuild mesh element set
    }

    return new_mesh;
  }

  /* Explicit template instantiations for common types */

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
} // namespace met
