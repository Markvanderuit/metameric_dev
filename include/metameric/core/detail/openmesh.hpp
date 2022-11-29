#pragma once

#include <metameric/core/detail/eigen.hpp>
#include <OpenMesh/Core/Mesh/Traits.hh>
#include <OpenMesh/Core/Mesh/Attributes.hh>
#include <OpenMesh/Core/Mesh/TriMesh_ArrayKernelT.hh>
#include <OpenMesh/Core/Geometry/EigenVectorT.hh>
#include <OpenMesh/Tools/Subdivider/Uniform/LoopT.hh>
#include <OpenMesh/Tools/Decimater/DecimaterT.hh>
#include <OpenMesh/Tools/Decimater/ModHausdorffT.hh>
#include <OpenMesh/Tools/Decimater/ModQuadricT.hh>
#include <OpenMesh/Tools/Decimater/ModEdgeLengthT.hh>

namespace met {
  namespace omesh = OpenMesh; // namespace shorthand
} // namespace met

namespace OpenMesh {
  struct BaseMeshTraits : public DefaultTraits {
    // Attribute types
    using Point      = Eigen::Vector3f;
    using Normal     = Eigen::Vector3f;
    using Color      = Eigen::Vector3f;
    using TexCoord2D = Eigen::Vector2f;

    // User-defined traits
    VertexTraits    {};
    HalfedgeTraits  {};
    EdgeTraits      {};
    FaceTraits      {};

    // Predefined attributes
    VertexAttributes(Attributes::None);
    EdgeAttributes(Attributes::None);
    HalfedgeAttributes(Attributes::PrevHalfedge);
    FaceAttributes(Attributes::Normal);
  };

  template <typename VectorType>
  struct VMeshTraits : public DefaultTraits {
    using Point  = VectorType;
    using Normal = VectorType;

    // User-defined traits
    VertexTraits    {};
    HalfedgeTraits  {};
    EdgeTraits      {};
    FaceTraits      {};

    // Predefined attributes
    VertexAttributes(Attributes::None);
    EdgeAttributes(Attributes::None);
    HalfedgeAttributes(Attributes::PrevHalfedge);
    FaceAttributes(Attributes::Normal);
  };

  using BaseMesh = TriMesh_ArrayKernelT<BaseMeshTraits>;

  template <typename VectorType>
  using VMesh = TriMesh_ArrayKernelT<VMeshTraits<VectorType>>;
} // namespace OpenMesh