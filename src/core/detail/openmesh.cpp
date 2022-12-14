#include <metameric/core/linprog.hpp>
#include <metameric/core/mesh.hpp>
#include <metameric/core/detail/openmesh.hpp>
#include <vector>
#if defined(OM_CC_MIPS)
#include <float.h>
#else
#include <cfloat>
#endif
#include <fmt/ranges.h>

/* 
eig::Vector3f solve_for_vertex(const std::vector<RealizedTriangle> &triangles,
                                const eig::Vector3f &min_v = 0.f,
                                const eig::Vector3f &max_v = 1.f) {
  met_trace();

  // Initialize parameter object for LP solver with expected matrix sizes
  LPParameters params(triangles.size(), 3);
  params.method    = LPMethod::ePrimal;
  params.objective = LPObjective::eMinimize;
  params.x_l       = min_v.cast<double>();
  params.x_u       = max_v.cast<double>();

  // Fill constraint matrices
  for (uint i = 0; i < triangles.size(); ++i) {
    eig::Vector3d n = (triangles[i].n).cast<double>().eval();
    params.A.row(i) = n;
    params.b[i] = n.dot(triangles[i].p0.cast<double>());
  }

  // Return minimized solution
  return lp_solve(params).cast<float>().eval();
} 
*/

namespace OpenMesh::Decimater {
  namespace detail {
    template <class MeshT>
    Vec3f solve_for_position(const CollapseInfoT<MeshT> &ci,
                             const Vec3f                &min_v = { 0, 0, 0 },
                             const Vec3f                &max_v = { 1, 1, 1 }) {
      auto &mesh = ci.mesh;
      auto &v0   = ci.v0;
      auto &v1   = ci.v1;

      // Left and right faces
      auto &fl   = ci.fl;
      auto &fr   = ci.fr;

      // Handles to surrounding faces
      auto f0 = mesh.vf_range(v0).to_vector();
      auto f1 = mesh.vf_range(v1).to_vector();

      // Initialize parameter object for LP solver with expected matrix sizes
      const uint N = 3;
      const uint M = f0.size() + f1.size() - 2; // 2 faces overlapping
      met::LPParameters params(M, N);
      params.method    = met::LPMethod::ePrimal;
      params.objective = met::LPObjective::eMinimize;
      params.x_l       = met::to_eig<float, 3>(min_v).cast<double>();
      params.x_u       = met::to_eig<float, 3>(max_v).cast<double>();

      // Fill constraint matrices: left face
      {
        auto nl = mesh.normal(fl), pl = mesh.point(ci.vl);
        params.A.row(0) = met::to_eig<float, 3>(nl).cast<double>();
        params.b[0] = nl.dot(pl);
      }

      // Fill constraint matrices: right face
      {
        auto nr = mesh.normal(fr), pr = mesh.point(ci.vr);
        params.A.row(1) = met::to_eig<float, 3>(nr).cast<double>();
        params.b[1] = nr.dot(pr);
      }
      
      // Fill constraint matrices: remaining faces
      uint i = 2;
      for (auto fh : f0) {
        guard_continue(fh.idx() != fl.idx() && fh.idx() != fr.idx());

        auto n = mesh.normal(fh), p = mesh.point(*fh.vertices().begin());
        params.A.row(i) = met::to_eig<float, 3>(n).cast<double>();
        params.b[i] = n.dot(p);
        i++;
      }
      for (auto fh : f1) {
        guard_continue(fh.idx() != fl.idx() && fh.idx() != fr.idx());

        auto n = mesh.normal(fh), p = mesh.point(*fh.vertices().begin());
        params.A.row(i) = met::to_eig<float, 3>(n).cast<double>();
        params.b[i] = n.dot(p);
        i++;
      }

      
      auto p = met::to_omesh<float, 3>(met::lp_solve(params).cast<float>().eval());
      fmt::print("{}\n", p);
      return p;
      // return 0.5f * (ci.p0 + ci.p1);
    }
  } // namespace detail

  template <class Mesh>
  VolumePreservingDecimater<Mesh>::VolumePreservingDecimater(Mesh& _mesh)
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

  template <class Mesh>
  VolumePreservingDecimater<Mesh>::~VolumePreservingDecimater() {
    // private vertex properties
    mesh_.remove_property(collapse_target_);
    mesh_.remove_property(priority_);
    mesh_.remove_property(heap_position_);
  }

  template<class Mesh>
  void VolumePreservingDecimater<Mesh>::heap_vertex(VertexHandle _vh) {
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
      //     std::clog << "  added|updated" << std::endl;
      mesh_.property(collapse_target_, _vh) = collapse_target;
      mesh_.property(priority_, _vh)        = best_prio;

      if (heap_->is_stored(_vh))
        heap_->update(_vh);
      else
        heap_->insert(_vh);
    }

    // not valid -> remove from heap
    else {
      //     std::clog << "  n/a|removed" << std::endl;
      if (heap_->is_stored(_vh))
        heap_->remove(_vh);

      mesh_.property(collapse_target_, _vh) = collapse_target;
      mesh_.property(priority_, _vh) = -1;
    }
  }

  template<class Mesh>
  size_t VolumePreservingDecimater<Mesh>::decimate(size_t _n_collapses, bool _only_selected) {
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

      // Solve for new volume-preserving position around half-edge
      auto p_new = detail::solve_for_position(ci);

      // perform collapse
      mesh_.collapse(v0v1);
      ++n_collapses;

      // Update remaining vertex to center
      mesh_.point(ci.v1) = p_new;

      if (update_normals)
      {
        // update triangle normals
        vf_it = mesh_.vf_iter(ci.v1);
        for (; vf_it.is_valid(); ++vf_it)
          if (!mesh_.status(*vf_it).deleted())
            mesh_.set_normal(*vf_it, mesh_.calc_face_normal(*vf_it));
      }

      // post-process collapse
      this->postprocess_collapse(ci);

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

  template<class Mesh>
  size_t VolumePreservingDecimater<Mesh>::decimate_to_faces(size_t _nv, size_t _nf, bool _only_selected) {
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

  /* explicit temlate instantiations */
  
  template class VolumePreservingDecimater<met::BaselineMesh>;
  template class VolumePreservingDecimater<met::FNormalMesh>;
  template class VolumePreservingDecimater<met::HalfedgeMesh>;
} // namespace OpenMesh::Decimater