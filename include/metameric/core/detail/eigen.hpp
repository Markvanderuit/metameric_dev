// Copyright (C) 2024 Mark van de Ruit, Delft University of Technology.
// 
// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.
// 
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
// 
// You should have received a copy of the GNU General Public License
// along with this program.  If not, see <https://www.gnu.org/licenses/>.

#pragma once

// Extensions to Eigen's existing classes are inserted through header files
#ifndef EIGEN_ARRAYBASE_PLUGIN
  #define EIGEN_ARRAYBASE_PLUGIN  "metameric/core/detail/ext/eigen_arraybase.ext"
#endif

#ifndef EIGEN_MATRIXBASE_PLUGIN
  #define EIGEN_MATRIXBASE_PLUGIN "metameric/core/detail/ext/eigen_matrixbase.ext"
#endif

#ifndef EIGEN_ARRAY_PLUGIN
  #define EIGEN_ARRAY_PLUGIN "metameric/core/detail/ext/eigen_array.ext"
#endif

#ifndef EIGEN_MATRIX_PLUGIN
  #define EIGEN_MATRIX_PLUGIN "metameric/core/detail/ext/eigen_matrix.ext"
#endif

#include <Eigen/Dense>
#include <Eigen/Geometry>

// Introduce 'eig' namespace shorthand in the metameric namespace
namespace met {
  namespace eig = Eigen;
} // namespace met

namespace Eigen {
  // Concept for detecting Eigen's isApprox(...) member function
  // on arbitrary types
  template <typename Ty>
  concept is_approx_comparable = requires(const Ty &a, const Ty &b) {
    { a.isApprox(b) } -> std::convertible_to<bool>;
  };

  // Eigen's blocks do not support single-component equality comparison,
  // but in general most things handle this just fine;
  // here's a nice hack for the few times this requires writing code twice
  template <typename Ty>
  bool safe_approx_compare(const Ty &a, const Ty &b) {
    if constexpr (is_approx_comparable<Ty>)
      return a.isApprox(b);
    else
      return a == b;
  }

  namespace detail {
    template <size_t D>
    constexpr 
    size_t vector_align() {
      return D >= 3 ? 16
           : D == 2 ? 8
           : 4;
    }

    // key_hash for eigen types for std::unordered_map/unordered_set
    template <typename T>
    constexpr
    auto matrix_hash = [](const auto &mat) {
      size_t seed = 0;
      for (size_t i = 0; i < mat.size(); ++i) {
        auto elem = *(mat.data() + i);
        seed ^= std::hash<T>()(elem) + 0x9e3779b9 + (seed << 6) + (seed >> 2);
      }
      return seed;
    };

    // key_equal for eigen types for std::unordered_map/unordered_set
    constexpr
    auto matrix_equal = [](const auto &a, const auto &b) { 
      return a.isApprox(b); 
    };

    template <typename Ty>
    struct matrix_hash_t {
      size_t operator()(const Ty &mat) const {
        size_t seed = 0;
        for (size_t i = 0; i < mat.size(); ++i) {
          auto elem = *(mat.data() + i);
          seed ^= std::hash<typename Ty::Scalar>()(elem) + 0x9e3779b9 + (seed << 6) + (seed >> 2);
        }
        return seed;
      }
    };

    template <typename Ty>
    struct matrix_equal_t { 
      bool operator()(const Ty &a, const Ty &b) const {
        return a.isApprox(b); 
      }
    };
  } // namespace detail

  template <class Type, size_t Size>
  class alignas(detail::vector_align<Size>()) AlArray 
  : public Array<Type, Size, 1> {
    using Base = Array<Type, Size, 1>;

  public:
    using Base::Base;

    AlArray() : Base() 
    { }

    // This constructor allows you to construct MyVectorType from Eigen expressions
    template <typename Other>
    AlArray(const ArrayBase<Other>& o)
    : Base(o)
    { }

    template<typename Other>
    AlArray& operator=(const ArrayBase <Other>& o)
    {
      this->Base::operator=(o);
      return *this;
    }
  };

  
  template <class Type, size_t Size>
  class alignas(detail::vector_align<Size>()) AlVector 
  : public Vector<Type, Size> {
    using Base = Vector<Type, Size>;

  public:
    AlVector() : Base() 
    { }

    // This constructor allows you to construct MyVectorType from Eigen expressions
    template <typename Other>
    AlVector(const MatrixBase<Other>& o)
    : Base(o)
    { }

    template<typename Other>
    AlVector& operator=(const MatrixBase <Other>& o)
    {
      this->Base::operator=(o);
      return *this;
    }
  };

  /* Define useful functions */

  // One-dimensional piecewise linear interpolation:
  // x  - array of sample points in [0, 1]
  // xp - array of data values to sample
  template <typename T, typename Tp>
  T interp(const T &x, const Tp &xp) {
    return x.unaryExpr([&xp](const float x) {
      float xa = (x * static_cast<float>(Tp::RowsAtCompileTime));
      float xl = static_cast<unsigned>(std::max(std::floor(xa), 0.f));
      float xu = static_cast<unsigned>(std::min(std::ceil(xa), static_cast<float>(Tp::RowsAtCompileTime - 1)));
      return std::lerp(xp[xl], xp[xu], xa - static_cast<float>(xl));
    }).eval();
  }

  // Usable cwiseMax/cwiseMin functions for algorithms/ranges ops/projs
  template <typename T> requires(is_approx_comparable<T>)
  T cwiseMax(const T &a, const T &b) { return a.cwiseMax(b).eval(); }
  template <typename T> requires(is_approx_comparable<T>)
  T cwiseMin(const T &a, const T &b) { return a.cwiseMin(b).eval(); }

  /* Define useful integer types */

  using Array1us = Array<unsigned short, 1, 1>;
  using Array2us = Array<unsigned short, 2, 1>;
  using Array3us = Array<unsigned short, 3, 1>;
  using Array4us = Array<unsigned short, 4, 1>;
  
  using Array1s = Array<short, 1, 1>;
  using Array2s = Array<short, 2, 1>;
  using Array3s = Array<short, 3, 1>;
  using Array4s = Array<short, 4, 1>;

  using Array1u = Array<unsigned int, 1, 1>;
  using Array2u = Array<unsigned int, 2, 1>;
  using Array3u = Array<unsigned int, 3, 1>;
  using Array4u = Array<unsigned int, 4, 1>;

  using Vector1u = Matrix<unsigned int, 1, 1>;
  using Vector2u = Matrix<unsigned int, 2, 1>;
  using Vector3u = Matrix<unsigned int, 3, 1>;
  using Vector4u = Matrix<unsigned int, 4, 1>;

  /* Define common aligned vector types */
  
  using AlArray3s  = AlArray<short, 3>;
  using AlArray3us = AlArray<unsigned short, 3>;
  using AlArray3u  = AlArray<unsigned int, 3>;
  using AlArray3i  = AlArray<int, 3>;
  using AlArray3f  = AlArray<float, 3>;
  using AlVector3f = AlVector<float, 3>;
  
  /* Define (sometimes) useful 1-component types */

  using Array1i  = Array<int, 1, 1>;
  using Array1u  = Array<unsigned int, 1, 1>;
  using Array1s  = Array<short, 1, 1>;
  using Array1us = Array<unsigned short, 1, 1>;
  using Array1f  = Array<float, 1, 1>;
  
} // namespace Eigen
