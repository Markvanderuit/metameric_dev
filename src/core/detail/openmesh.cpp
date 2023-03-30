#include <metameric/core/linprog.hpp>
#include <metameric/core/mesh.hpp>
#include <metameric/core/ray.hpp>
#include <metameric/core/detail/openmesh.hpp>
#include <algorithm>
#include <execution>
#include <vector>
#if defined(OM_CC_MIPS)
#include <float.h>
#else
#include <cfloat>
#endif

namespace met {
  template <>
  eig::Array3f convert_vector<eig::Array3f, omesh::Vec3f>(const omesh::Vec3f &v) {
    return eig::Array3f(v.data());
  }

  template <>
  eig::AlArray3f convert_vector<eig::AlArray3f, omesh::Vec3f>(const omesh::Vec3f &v) {
    return convert_vector<eig::Array3f, omesh::Vec3f>(v);
  }

  template <>
  omesh::Vec3f convert_vector<omesh::Vec3f, eig::Array3f>(const eig::Array3f &v) {
    return omesh::Vec3f(v.begin());
  }
  
  template <>
  omesh::Vec3f convert_vector<omesh::Vec3f, eig::AlArray3f>(const eig::AlArray3f &v) {
    return convert_vector<omesh::Vec3f, eig::Array3f>(v);
  }
} // namespace met

namespace OpenMesh::Decimater {
  namespace detail {
    template <class MeshT>
    std::pair<float, Vec3f> volume_solve(const MeshT &mesh,
                                         const MeshT *bounds_mesh,
                                         const Vec3f &bounds_cntr,
                                         const typename MeshT::HalfedgeHandle &v0v1,
                                         const Vec3f &min_v = { 0, 0, 0 },
                                         const Vec3f &max_v = { 1, 1, 1 }) {
      auto v1v0 = mesh.opposite_halfedge_handle(v0v1);

      // Vertex handles, positions
      auto v0 = mesh.to_vertex_handle(v1v0), v1 = mesh.to_vertex_handle(v0v1);
      auto p0 = mesh.point(v0), p1 = mesh.point(v1);
      guard(p0 != p1, { 0.f, p1 });

      // Left/right face handles
      auto fl = mesh.face_handle(v0v1), fr = mesh.face_handle(v1v0);

      // Merge handles to surrounding faces
      auto f0 = mesh.vf_range(v0).to_vector(), f1 = mesh.vf_range(v1).to_vector();
      std::vector<decltype(f0)::value_type> fm(f0.size() + f1.size() - 2);
      std::copy(range_iter(f0), fm.begin());
      std::copy_if(range_iter(f1), fm.begin() + f0.size(), 
        [&fl, &fr](auto h) { return h.idx() != fl.idx() && h.idx() != fr.idx(); });

      // Expected matrix sizes
      const uint N = 3;
      const uint M = fm.size(); // + 6;
      
      // Initialize parameter object for LP solver with expected matrix sizes
      met::LPParameters params(M, N);
      params.method    = met::LPMethod::ePrimal;
      params.objective = met::LPObjective::eMinimize;
      params.r         = met::LPCompare::eLE;

      // Fill constraint matrices describing added volume, which must be zero or positive
      Vec3f n_sum = { 0, 0, 0 };
      uint i = 0;
      for (auto fh : fm) {
        // Get vertex positions and edge lengths
        std::array<Vec3f, 3> p;
        std::array<float, 3> d;
        std::ranges::transform(fh.vertices(), p.begin(), [&](auto vh) { return mesh.point(vh); }); 
        for (uint i = 0; i < 3; ++i) d[i] = (p[(i + 1) % 3] - p[i]).length();

        // Skip collapsed triangles
        guard_continue(std::ranges::none_of(d, [](float f) { return f == 0.f; }));

        // Compute triangle area
        auto s = 0.5f * (d[0] + d[1] + d[2]);
        auto A = std::sqrtf(s * (s - d[0]) * (s - d[1]) * (s - d[2]));

        // Compute volume metric
        auto n = -mesh.calc_face_normal(fh);
        n = n / std::sqrtf(n.dot(n));
        n = (A / 3.f) * n;

        // Add normal to objective function
        n_sum += n;

        // Set constraint information
        params.A.row(i) = met::to_eig<float, 3>(n).cast<double>().eval();
        params.b[i]     = n.dot(p[0]); 

        i++;
      }

      // Minimize the solution vertex with respect to triangle normals, which places it
      // on their intersecting planes; run solver, and pray 
      params.C = met::to_eig<float, 3>(n_sum).cast<double>().eval();
      auto [optimal, solution] = met::lp_solve_res(params);
      auto vertex = met::to_omesh<float, 3>(solution.cast<float>());

      // Penalize failed solutions
      if (!optimal)
        return { 99999.f, vertex };

      // Reproject vertex onto boundary mesh surface if it exceeds this
      if (bounds_mesh) {
        // Cast a ray through the vertex point towards the mesh centroid;
        met::Ray ray = { .o = met::to_eig<float, 3>(vertex),
                         .d = met::to_eig<float, 3>(bounds_cntr - vertex).matrix().normalized().eval() };
        auto query = met::raytrace_elem(ray, *bounds_mesh, false);
        if (query) {
          // Get relevant face
          auto fh = bounds_mesh->face_handle(query.i);
          if (fh.is_valid()) {
            // Test if face is facing towards us, or if we are on the inside
            auto n = met::to_eig<float, 3>(bounds_mesh->calc_face_normal(fh));
            if (n.dot(ray.d) < 0.f) {
              vertex = met::to_omesh<float, 3>(ray.o + query.t * ray.d);
            }
          }
        }
      }

      // Epsilon offset for solver problems
      if (vertex == Vec3f { 0, 0, 0 })
        vertex += Vec3f(0.00001f);

      float volume = 0.f;
      for (auto fh : fm) {
        // Get vertex positions and edge lengths
        std::array<Vec3f, 3> p;
        std::array<float, 3> d;
        std::ranges::transform(fh.vertices(), p.begin(), [&](auto vh) { return mesh.point(vh); }); 
        for (uint i = 0; i < 3; ++i) d[i] = (p[(i + 1) % 3] - p[i]).length();

        // Skip collapsed triangles
        guard_continue(std::ranges::none_of(d, [](float f) { return f == 0.f; }));

        // Compute triangle area
        auto s = 0.5f * (d[0] + d[1] + d[2]);
        auto A = std::sqrtf(s * (s - d[0]) * (s - d[1]) * (s - d[2]));

        // Compute volume metric
        auto n = (A / 3.f) * mesh.calc_face_normal(fh);
        auto v = n.dot(vertex - p[0]);
        volume += std::abs(v);
      }

      return { volume, vertex };
    }
  } // namespace detail

  template <class Mesh, template <typename> typename CollapseFunc>
  CollapsingDecimater<Mesh, CollapseFunc>::CollapsingDecimater(Mesh& _mesh)
  : BaseDecimaterT<Mesh>(_mesh),
    mesh_(_mesh),
  #if (defined(_MSC_VER) && (_MSC_VER >= 1800)) || __cplusplus > 199711L || defined( __GXX_EXPERIMENTAL_CXX0X__ )
    heap_(nullptr)
  #else
    heap_(nullptr)
  #endif
  {
    // private vertex properties
    mesh_.add_property(collapse_target_);
    mesh_.add_property(priority_);
    mesh_.add_property(heap_position_);
  }

  template <class Mesh, template <typename> typename CollapseFunc>
  CollapsingDecimater<Mesh, CollapseFunc>::~CollapsingDecimater() {
    // private vertex properties
    mesh_.remove_property(collapse_target_);
    mesh_.remove_property(priority_);
    mesh_.remove_property(heap_position_);
  }

  template<class Mesh, template <typename> typename CollapseFunc>
  void CollapsingDecimater<Mesh, CollapseFunc>::heap_vertex(VertexHandle _vh) {
    float prio, best_prio(FLT_MAX);
    typename Mesh::HalfedgeHandle heh, collapse_target;

    // find best target in one ring
    typename Mesh::VertexOHalfedgeIter voh_it(mesh_, _vh);
    for (; voh_it.is_valid(); ++voh_it) {
      heh = *voh_it;
      CollapseInfo ci(mesh_, heh);

      if (this->is_collapse_legal(ci)) {
        prio = this->collapse_priority(ci);
        if (prio >= 0.0 && prio < best_prio) {
          best_prio = prio;
          collapse_target = heh;
        }
      }
    }

    // target found -> put vertex on heap
    if (collapse_target.is_valid()) {
          // std::clog << "  added|updated" << std::endl;
      mesh_.property(collapse_target_, _vh) = collapse_target;
      mesh_.property(priority_, _vh)        = best_prio;

      if (heap_->is_stored(_vh))
        heap_->update(_vh);
      else
        heap_->insert(_vh);
    }

    // not valid -> remove from heap
    else {
          // std::clog << "  n/a|removed" << std::endl;
      if (heap_->is_stored(_vh))
        heap_->remove(_vh);

      mesh_.property(collapse_target_, _vh) = collapse_target;
      mesh_.property(priority_, _vh) = -1;
    }
  }

  template<class Mesh, template <typename> typename CollapseFunc>
  size_t CollapsingDecimater<Mesh, CollapseFunc>::decimate(size_t _n_collapses, bool _only_selected) {
    if (!this->is_initialized())
      return 0;

    typename Mesh::VertexHandle vp;
    typename Mesh::HalfedgeHandle v0v1;
    typename Mesh::VertexVertexIter vv_it;
    typename Mesh::VertexFaceIter vf_it;
    unsigned int n_collapses(0);

    typedef std::vector<typename Mesh::VertexHandle> Support;
    typedef typename Support::iterator SupportIterator;

    Support support(15);
    SupportIterator s_it, s_end;

    // check _n_collapses
    if (!_n_collapses)
      _n_collapses = mesh_.n_vertices();

    // initialize heap
    HeapInterface HI(mesh_, priority_, heap_position_);

  #if (defined(_MSC_VER) && (_MSC_VER >= 1800)) || __cplusplus > 199711L || defined( __GXX_EXPERIMENTAL_CXX0X__ )
    heap_ = std::unique_ptr<DeciHeap>(new DeciHeap(HI));
  #else
    heap_ = std::auto_ptr<DeciHeap>(new DeciHeap(HI));
  #endif

    heap_->reserve(mesh_.n_vertices());

    for ( auto v_it : mesh_.vertices() ) {
      heap_->reset_heap_position(v_it);

      if (!mesh_.status(v_it).deleted()) {
        if (!_only_selected  || mesh_.status(v_it).selected() ) {
          heap_vertex(v_it);
        }
      }
    }

    const bool update_normals = mesh_.has_face_normals();

    // process heap
    while ((!heap_->empty()) && (n_collapses < _n_collapses)) {
      // get 1st heap entry
      vp = heap_->front();
      v0v1 = mesh_.property(collapse_target_, vp);
      heap_->pop_front();

      // setup collapse info
      CollapseInfo ci(mesh_, v0v1);

      // check topological correctness AGAIN !
      if (!this->is_collapse_legal(ci))
        continue;

      // store support (= one ring of *vp)
      vv_it = mesh_.vv_iter(ci.v0);
      support.clear();
      for (; vv_it.is_valid(); ++vv_it)
        support.push_back(*vv_it);

      // pre-processing
      this->preprocess_collapse(ci);

      // Obtain collapsed position
      auto p_new = CollapseFunction::collapse(ci);

      // perform collapse
      mesh_.collapse(v0v1);
      ++n_collapses;

      // Set collapsed position
      mesh_.point(ci.v1) = p_new;

      // post-process collapse
      this->postprocess_collapse(ci);

      // update triangle normals
      if (update_normals) {
        vf_it = mesh_.vf_iter(ci.v1);
        for (; vf_it.is_valid(); ++vf_it)
          if (!mesh_.status(*vf_it).deleted())
            mesh_.set_normal(*vf_it, mesh_.calc_face_normal(*vf_it));
      }

      // update heap; new vertex and surrounding supports needs refitting
      heap_vertex(ci.v1);
      for (auto vh : mesh_.vv_range(ci.v1)) {
        met::debug::check_expr(vh.idx() != ci.v0.idx(), "v0 is present!");
        assert(!mesh_.status(vh).deleted());
        if (!_only_selected  || mesh_.status(vh).selected() )
          heap_vertex(vh);

        // Given volume metric, support of support also needs refitting 
        for (auto vh_ : mesh_.vv_range(vh)) {
          guard_continue(vh_.idx() != vh.idx());
          assert(!mesh_.status(vh_).deleted());
          if (!_only_selected || mesh_.status(vh_).selected() )
            heap_vertex(vh_);
        }
      }

      // update heap (former one ring of decimated vertex)
      for (s_it = support.begin(), s_end = support.end(); s_it != s_end; ++s_it) {
        assert(!mesh_.status(*s_it).deleted());
        if (!_only_selected  || mesh_.status(*s_it).selected() )
          heap_vertex(*s_it);
      }

      // notify observer and stop if the observer requests it
      if (!this->notify_observer(n_collapses))
          return n_collapses;
    }

    // delete heap
    heap_.reset();

    // DON'T do garbage collection here! It's up to the application.
    return n_collapses;
  }

  template<class Mesh, template <typename> typename CollapseFunc>
  size_t CollapsingDecimater<Mesh, CollapseFunc>::decimate_to_faces(size_t _nv, size_t _nf, bool _only_selected) {
    if (!this->is_initialized())
      return 0;

    if (_nv >= mesh_.n_vertices() || _nf >= mesh_.n_faces())
      return 0;

    typename Mesh::VertexHandle vp;
    typename Mesh::HalfedgeHandle v0v1;
    typename Mesh::VertexVertexIter vv_it;
    typename Mesh::VertexFaceIter vf_it;
    size_t nv = mesh_.n_vertices();
    size_t nf = mesh_.n_faces();
    unsigned int n_collapses = 0;

    typedef std::vector<typename Mesh::VertexHandle> Support;
    typedef typename Support::iterator SupportIterator;

    Support support(15);
    SupportIterator s_it, s_end;

    // initialize heap
    HeapInterface HI(mesh_, priority_, heap_position_);
    #if (defined(_MSC_VER) && (_MSC_VER >= 1800)) || __cplusplus > 199711L || defined( __GXX_EXPERIMENTAL_CXX0X__ )
      heap_ = std::unique_ptr<DeciHeap>(new DeciHeap(HI));
    #else
      heap_ = std::auto_ptr<DeciHeap>(new DeciHeap(HI));
    #endif
    heap_->reserve(mesh_.n_vertices());

    for ( auto v_it : mesh_.vertices() ) {
      heap_->reset_heap_position(v_it);
      if (!mesh_.status(v_it).deleted()) {
        if (!_only_selected  || mesh_.status(v_it).selected() ) {
          heap_vertex(v_it);
        }
      }
    }

    const bool update_normals = mesh_.has_face_normals();

    // process heap
    while ((!heap_->empty()) && (_nv < nv) && (_nf < nf)) {
      // get 1st heap entry
      vp = heap_->front();
      v0v1 = mesh_.property(collapse_target_, vp);
      heap_->pop_front();

      // setup collapse info
      CollapseInfo ci(mesh_, v0v1);

      // check topological correctness AGAIN !
      if (!this->is_collapse_legal(ci))
        continue;

      // store support (= one ring of *vp)
      vv_it = mesh_.vv_iter(ci.v0);
      support.clear();
      for (; vv_it.is_valid(); ++vv_it)
        support.push_back(*vv_it);

      // adjust complexity in advance (need boundary status)
      ++n_collapses;
      --nv;
      if (mesh_.is_boundary(ci.v0v1) || mesh_.is_boundary(ci.v1v0))
        --nf;
      else
        nf -= 2;

      // pre-processing
      this->preprocess_collapse(ci);

      // perform collapse
      mesh_.collapse(v0v1);

      // update triangle normals
      if (update_normals)
      {
        vf_it = mesh_.vf_iter(ci.v1);
        for (; vf_it.is_valid(); ++vf_it)
          if (!mesh_.status(*vf_it).deleted())
            mesh_.set_normal(*vf_it, mesh_.calc_face_normal(*vf_it));
      }

      // post-process collapse
      this->postprocess_collapse(ci);

      // Update heap (remaining, moved vertex and its one ring)
      // heap_vertex(ci.v0);
      heap_vertex(ci.v1);
      /* for (auto vh : mesh_.vv_range(ci.v1)) {
        assert(!mesh_.status(vh).deleted());
        if (!_only_selected  || mesh_.status(vh).selected() ) {
          heap_vertex(vh);
        }
      }
      for (auto vh : mesh_.vv_range(ci.v0)) {
        met::debug::check_expr(vh.idx() != ci.v0.idx(), "v0 is present!");
        assert(!mesh_.status(vh).deleted());
        if (!_only_selected  || mesh_.status(vh).selected() ) {
          heap_vertex(vh);
        }
      } */
      for (auto vh : mesh_.vv_range(ci.v1)) {
        met::debug::check_expr(vh.idx() != ci.v0.idx(), "v0 is present!");
        assert(!mesh_.status(vh).deleted());
        if (!_only_selected  || mesh_.status(vh).selected() )
          heap_vertex(vh);
        for (auto vh_ : mesh_.vv_range(ci.v1)) {
          guard_continue(vh_.idx() != ci.v1.idx());
          assert(!mesh_.status(vh_).deleted());
          if (!_only_selected  || mesh_.status(vh_).selected() )
            heap_vertex(vh_);
        }
      }

      // update heap (former one ring of decimated vertex)
      for (s_it = support.begin(), s_end = support.end(); s_it != s_end; ++s_it) {
        assert(!mesh_.status(*s_it).deleted());
        if (!_only_selected  || mesh_.status(*s_it).selected() )
          heap_vertex(*s_it);
      }

      // notify observer and stop if the observer requests it
      if (!this->notify_observer(n_collapses))
          return n_collapses;
    }

    // delete heap
    heap_.reset();

    // DON'T do garbage collection here! It's up to the application.
    return n_collapses;
  }

  template <typename MeshT>
  void ModVolumeT<MeshT>::initialize() {
    // Solve volume-preserving vertex and resulting added volume for each half-edge
    for (auto eh : m_mesh.edges()) {
      // Relevant half-edges
      auto v0v1 = eh.h0(), v1v0 = eh.h1();

      // Solve for one half-edge, results should be identical for both
      auto solve = detail::volume_solve<MeshT>(m_mesh, m_collision_mesh, m_collision_centroid, v0v1);

      std::tie(m_mesh.property(m_volume, v0v1), 
               m_mesh.property(m_vertex, v0v1)) = solve;
      std::tie(m_mesh.property(m_volume, v1v0), 
               m_mesh.property(m_vertex, v1v0)) = solve;
    }
  }
  
  template <typename MeshT>
  float ModVolumeT<MeshT>::collapse_priority(const CollapseInfoT<MeshT>& ci) {
    // Return pre-computed added volume, should this half-edge be collapsed
    auto volume = m_mesh.property(m_volume, ci.v0v1);
    return (volume >= 0.f && volume < m_maximum_volume) 
      ? volume 
      : 99999.f; // float(Base::ILLEGAL_COLLAPSE);
  }
  
  template <typename MeshT>
  void ModVolumeT<MeshT>::postprocess_collapse(const CollapseInfoT<MeshT>& ci) {
    // Assign volume-preserving position to remaining uncollapsed vertex position
    m_mesh.point(ci.v1) = m_mesh.property(m_vertex, ci.v0v1);

    // Rerun solve for each edge around affected vertex support
    for (auto vh : m_mesh.vv_range(ci.v1)) {
      for (auto eh : m_mesh.ve_range(vh)) {
        // Relevant half-edges
        auto v0v1 = eh.h0(), v1v0 = eh.h1();

        // Solve for one half-edge, results should be identical for both
        auto solve = detail::volume_solve(m_mesh, m_collision_mesh, m_collision_centroid, v0v1);

        // Store resulting volume/vertex in halfedge properties
        std::tie(m_mesh.property(m_volume, v0v1), 
                 m_mesh.property(m_vertex, v0v1)) = solve;
        std::tie(m_mesh.property(m_volume, v1v0), 
                 m_mesh.property(m_vertex, v1v0)) = solve;
      }
    }
  }

  template <typename MeshT>
  void ModVolumeT<MeshT>::set_collision_mesh(const MeshT *mesh) {
    constexpr auto f_add = [](const auto &a, const auto &b) { return a + b; };

    m_collision_mesh = mesh;
    m_collision_centroid = std::reduce(std::execution::par_unseq, 
      mesh->points(), mesh->points() + mesh->n_vertices(),
      Vec3f(0.f), f_add) / static_cast<float>(mesh->n_vertices());
  }


  /* explicit temlate instantiations */

  template class ModVolumeT<met::HalfedgeMeshData>;
  template class CollapsingDecimater<met::HalfedgeMeshData, DefaultCollapseFunction>;
  template class CollapsingDecimater<met::HalfedgeMeshData, AverageCollapseFunction>;
} // namespace OpenMesh::Decimater