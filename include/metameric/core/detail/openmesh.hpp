#pragma once

#include <metameric/core/detail/eigen.hpp>
#include <OpenMesh/Core/Mesh/Traits.hh>
#include <OpenMesh/Core/Mesh/Attributes.hh>
#include <OpenMesh/Core/Mesh/TriMesh_ArrayKernelT.hh>
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