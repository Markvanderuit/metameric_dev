#pragma once

#include <metameric/core/detail/eigen.hpp>
#include <OpenMesh/Core/Utils/Property.hh>
#include <OpenMesh/Core/Mesh/Traits.hh>
#include <OpenMesh/Core/Mesh/Attributes.hh>
#include <OpenMesh/Core/Mesh/TriMesh_ArrayKernelT.hh>
#include <OpenMesh/Tools/Utils/HeapT.hh>
#include <OpenMesh/Tools/Decimater/BaseDecimaterT.hh>
#include <memory>
#include <ranges>

namespace met {
  namespace omesh = OpenMesh; // namespace shorthand

  template <typename Scalar, int Rows>
  eig::Vector<Scalar, Rows> to_eig(const omesh::VectorT<Scalar, Rows> &v) {
    eig::Vector<Scalar, Rows> _v;
    std::ranges::copy(v, _v.begin());
    return _v;
  }

  template <typename Scalar, int Rows>
  omesh::VectorT<Scalar, Rows> to_omesh(const eig::Vector<Scalar, Rows> &v) {
    omesh::VectorT<Scalar, Rows> _v;
    std::ranges::copy(v, _v.begin());
    return _v;
  }
} // namespace met

namespace OpenMesh::Decimater {
  /**
   * Implementation of volume preserving mesh decimater, integrating with the decimater system
   * used in OpenMesh. Mostly copy of OpenNesh's DecimaterT class, barring minor changes.
   * https://gitlab.vci.rwth-aachen.de:9000/OpenMesh/OpenMesh/-/blob/master/src/OpenMesh/Tools/Decimater/DecimaterT.hh
  */
  template <typename MeshT>
  class VolumePreservingDecimater : virtual public BaseDecimaterT<MeshT> {
  public: /* public types */
    typedef VolumePreservingDecimater<MeshT> Self;
    typedef MeshT                            Mesh;
    typedef CollapseInfoT<MeshT>             CollapseInfo;
    typedef ModBaseT<MeshT>                  Module;
    typedef std::vector< Module* >           ModuleList;
    typedef typename ModuleList::iterator    ModuleListIterator;

  public: /* public methods */
    // Constr/destr
    explicit VolumePreservingDecimater(Mesh& _mesh);
    ~VolumePreservingDecimater();

    size_t decimate(size_t _n_collapses = 0, bool _only_selected = false);
    size_t decimate_to_faces(size_t _n_vertices=0, size_t _n_faces=0 , bool _only_selected = false);
    size_t decimate_to(size_t _n_vertices, bool _only_selected = false) {
      return ((_n_vertices < this->mesh().n_vertices()) ?
        decimate(this->mesh().n_vertices() - _n_vertices , _only_selected) : 0);
    }

  public: /* heap interface */
    typedef typename Mesh::VertexHandle    VertexHandle;
    typedef typename Mesh::HalfedgeHandle  HalfedgeHandle;

    class HeapInterface {
    public:
      HeapInterface(Mesh&   _mesh,
        VPropHandleT<float> _prio,
        VPropHandleT<int>   _pos)
        : mesh_(_mesh), prio_(_prio), pos_(_pos)
      { }

      inline 
      bool less( VertexHandle _vh0, VertexHandle _vh1 ) { 
        return mesh_.property(prio_, _vh0) < mesh_.property(prio_, _vh1);
      }

      inline 
      bool greater( VertexHandle _vh0, VertexHandle _vh1 ) {
        return mesh_.property(prio_, _vh0) > mesh_.property(prio_, _vh1);
      }

      inline 
      int get_heap_position(VertexHandle _vh) {
        return mesh_.property(pos_, _vh); 
      }

      inline 
      void set_heap_position(VertexHandle _vh, int _pos) {
        mesh_.property(pos_, _vh) = _pos;
      }

    private:
      Mesh&                mesh_;
      VPropHandleT<float>  prio_;
      VPropHandleT<int>    pos_;
    };

    typedef Utils::HeapT<VertexHandle, HeapInterface>  DeciHeap;
  
  private: /* private methods */
    void heap_vertex(VertexHandle _vh);

  private: /* private data */
    // reference to mesh
    Mesh&      mesh_;

    // heap
    #if (defined(_MSC_VER) && (_MSC_VER >= 1800)) || __cplusplus > 199711L || defined( __GXX_EXPERIMENTAL_CXX0X__ )
      std::unique_ptr<DeciHeap> heap_;
    #else
      std::auto_ptr<DeciHeap> heap_;
    #endif

    // vertex properties
    VPropHandleT<HalfedgeHandle>  collapse_target_;
    VPropHandleT<float>           priority_;
    VPropHandleT<int>             heap_position_;
  };
} // namespace OpenMesh::Decimater