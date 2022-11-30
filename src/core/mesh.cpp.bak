#include <metameric/core/mesh.hpp>
#include <metameric/core/utility.hpp>
#include <metameric/core/detail/openmesh.hpp>
#include <metameric/core/detail/trace.hpp>
#include <OpenMesh/Tools/Subdivider/Uniform/LoopT.hh>
#include <OpenMesh/Core/Geometry/EigenVectorT.hh>
#include <OpenMesh/Tools/Decimater/DecimaterT.hh>
#include <OpenMesh/Tools/Decimater/ModQuadricT.hh>
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

  /* template <typename T, typename E>
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
  } */

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

  template <typename T, typename E>
  T IndexedMesh<T, E>::centroid() const {
    constexpr auto f_add = [](const auto &a, const auto &b) { return (a + b).eval(); };
    return std::reduce(std::execution::par_unseq, range_iter(m_verts), T(0.f), f_add) / static_cast<float>(m_verts.size());
  }
  
  template <typename T, typename T_>
  IndexedMesh<T, eig::Array3u> from_omesh(const omesh::VMesh<T_> &o) {
    met_trace();

    std::vector<T>            verts(o.n_vertices());
    std::vector<eig::Array3u> elems(o.n_faces());

    std::transform(std::execution::par_unseq, range_iter(o.vertices()), verts.begin(), 
      [&](auto vh) { return T(o.point(vh)); });
    std::transform(std::execution::par_unseq, range_iter(o.faces()), elems.begin(), [](auto fh) {
      eig::Array3u el; 
      std::ranges::transform(fh.vertices(), el.begin(), [](auto vh) { return vh.idx(); });
      return el;
    });

    return { verts, elems };
  }
  
  template <typename T>
  IndexedMesh<T, eig::Array3u> from_omesh_defualt(const omesh::TriMesh_ArrayKernelT<> &o) {
    met_trace();

    std::vector<T>            verts(o.n_vertices());
    std::vector<eig::Array3u> elems(o.n_faces());

    std::transform(std::execution::par_unseq, range_iter(o.vertices()), verts.begin(), [&](auto vh) { 
      auto p = o.point(vh);
      return T(p[0], p[1], p[2]);
    });
    std::transform(std::execution::par_unseq, range_iter(o.faces()), elems.begin(), [](auto fh) {
      eig::Array3u el; 
      std::ranges::transform(fh.vertices(), el.begin(), [](auto vh) { return vh.idx(); });
      return el;
    });

    return { verts, elems };
  }

  template <typename T>
  omesh::TriMesh_ArrayKernelT<> to_omesh_default(const IndexedMesh<T, eig::Array3u> &o) {
    met_trace();

    omesh::TriMesh_ArrayKernelT<> mesh;
    mesh.reserve(o.verts().size(), (o.verts().size() + o.elems().size() - 2), o.elems().size());
    
    std::vector<typename omesh::TriMesh_ArrayKernelT<>::VertexHandle> vth(o.verts().size());
    std::ranges::transform(o.verts(), vth.begin(), [&](const auto &v) { return mesh.add_vertex(omesh::Vec3f(v[0], v[1], v[2])); });
    std::ranges::for_each(o.elems(), [&](const auto &e) { return mesh.add_face(vth[e[0]], vth[e[1]], vth[e[2]]); });

    return mesh;
  }

  template <typename T, typename T_>
  omesh::VMesh<T> to_omesh(const IndexedMesh<T_, eig::Array3u> &o) {
    met_trace();

    omesh::VMesh<T> mesh;
    mesh.reserve(o.verts().size(), (o.verts().size() + o.elems().size() - 2), o.elems().size());
    
    std::vector<typename omesh::VMesh<T>::VertexHandle> vth(o.verts().size());
    std::ranges::transform(o.verts(), vth.begin(), [&](const auto &v) { return mesh.add_vertex(T(v)); });
    std::ranges::for_each(o.elems(), [&](const auto &e) { return mesh.add_face(vth[e[0]], vth[e[1]], vth[e[2]]); });

    return mesh;
  }

  template <typename V, typename E>
  IndexedMesh<V, E> generate_octahedron() {
    met_trace();
    std::array<V, 6> vts = { V(-1.f, 0.f, 0.f ), V( 0.f,-1.f, 0.f ), V( 0.f, 0.f,-1.f ),
                             V( 1.f, 0.f, 0.f ), V( 0.f, 1.f, 0.f ), V( 0.f, 0.f, 1.f ) };
    std::array<E, 8> els = { E(0, 1, 2), E(3, 2, 1), E(0, 5, 1), E(3, 1, 5),
                             E(0, 4, 5), E(3, 5, 4), E(0, 2, 4), E(3, 4, 2) };
    return { vts, els };
  }

  template <typename T>
  IndexedMesh<T, eig::Array3u> generate_unit_sphere(uint n_subdivs) {
    met_trace();

    using Vect = eig::Vector3f;
    using Mesh = omesh::VMesh<Vect>;

    // Initial mesh describes an octahedron
    Mesh mesh = to_omesh<Vect, T>(generate_octahedron<T, eig::Array3u>());

    // Perform lopp subdivision n times
    omesh::Subdivider::Uniform::LoopT<Mesh, float> subdivider;
    subdivider.attach(mesh);
    subdivider(n_subdivs);
    subdivider.detach();

    // Normalize each vertex point s.t. they become unit vectors on a sphere
    std::for_each(std::execution::par_unseq, range_iter(mesh.vertices()),
      [&](auto vh) { mesh.point(vh).normalize(); });

    return from_omesh<T>(mesh);
  }

  template <typename T>
  IndexedMesh<T, eig::Array3u> simplify_mesh(const IndexedMesh<T, eig::Array3u> &complex, uint max_vertices) {
    met_trace();

    using Vect = eig::Vector3f;
    using Mesh = omesh::TriMesh_ArrayKernelT<>;

    // Initialize mesh
    Mesh mesh = to_omesh_default<T>(complex);
    mesh.request_face_normals();
    mesh.update_face_normals();

    // Perform mesh decimation until curr_vertices <= max_vertices is satisfied
    omesh::Decimater::DecimaterT<Mesh> decimater(mesh);
    omesh::Decimater::ModQuadricT<Mesh>::Handle mod_quadric_t;
    // omesh::Decimater::ModHausdorffT<Mesh>::Handle mod_haussdorf;
    decimater.add(mod_quadric_t);
    decimater.module(mod_quadric_t).unset_max_err();
    // decimater.add(mod_haussdorf);
    // decimater.module(mod_haussdorf).set_binary(false);
    decimater.initialize();
    decimater.decimate_to(max_vertices);

    mesh.garbage_collection();

    return from_omesh_defualt<T>(mesh);
  }


  /* template <typename T>
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
  } */


  /* Explicit template instantiations for common types */

  template class IndexedMesh<eig::Array3f, eig::Array3u>;
  template class IndexedMesh<eig::AlArray3f, eig::Array3u>;

  template IndexedMesh<eig::Array3f, eig::Array3u>   generate_unit_sphere<eig::Array3f>(uint);
  template IndexedMesh<eig::AlArray3f, eig::Array3u> generate_unit_sphere<eig::AlArray3f>(uint);

  template IndexedMesh<eig::Array3f, eig::Array2u>
  generate_wireframe<eig::Array3f>(const IndexedMesh<eig::Array3f, eig::Array3u> &);
  template IndexedMesh<eig::AlArray3f, eig::Array2u>
  generate_wireframe<eig::AlArray3f>(const IndexedMesh<eig::AlArray3f, eig::Array3u> &);

  template IndexedMesh<eig::Array3f, eig::Array3u> 
  simplify_mesh<eig::Array3f>(const IndexedMesh<eig::Array3f, eig::Array3u> &, uint);
  template IndexedMesh<eig::AlArray3f, eig::Array3u> 
  simplify_mesh<eig::AlArray3f>(const IndexedMesh<eig::AlArray3f, eig::Array3u> &, uint);
} // namespace met
