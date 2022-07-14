#pragma once

// Extensions to Eigen's existing classes are inserted through header files
#define EIGEN_ARRAYBASE_PLUGIN  "eigen_arraybase.ext"
#define EIGEN_MATRIXBASE_PLUGIN "eigen_matrixbase.ext"
#define EIGEN_ARRAY_PLUGIN      "eigen_array.ext"
#define EIGEN_MATRIX_PLUGIN     "eigen_matrix.ext"

#include <metameric/core/fwd.hpp>
#include <Eigen/Dense>

namespace met {
  namespace detail {
    template <uint D>
    constexpr uint vector_align() {
      static_assert(D > 0 && D <= 4);
      return D == 4 ? 16
           : D == 3 ? 16
           : D == 2 ? 8
           : 4;
    }

    template <uint D>
    constexpr uint _matrix_align() {
      static_assert(D > 0 && D <= 4);
      return D == 4 ? 4
           : D == 3 ? 4
           : D == 2 ? 2
           : 1;
    }

    template <uint R, uint C>
    constexpr uint matrix_align() {
      return _matrix_align<R>() * vector_align<C>();
    }
  } // namespace detail
  
  // namespace shorthand
  namespace eig = Eigen;

  template <class Type, size_t Rows, size_t Cols>
  class alignas(detail::matrix_align<Rows, Cols>()) AlignedMatrix 
  : public eig::Matrix<Type, Rows, Cols> {
    using Base = eig::Matrix<Type, Rows, Cols>;

  public:
    AlignedMatrix() : Base() 
    { }

    // This constructor allows you to construct MyVectorType from Eigen expressions
    template <typename Other>
    AlignedMatrix(const eig::MatrixBase<Other>& o)
    : Base(o)
    { }

    template<typename Other>
    AlignedMatrix& operator=(const eig::MatrixBase <Other>& o)
    {
      this->Base::operator=(o);
      return *this;
    }
  };

  template <class Type, size_t Rows, size_t Cols>
  class alignas(detail::matrix_align<Rows, Cols>()) AlignedArray 
  : public eig::Array<Type, Rows, Cols> {
    using Base = eig::Array<Type, Rows, Cols>;

  public:
    using Base::Base;

    AlignedArray() : Base() 
    { }

    // This constructor allows you to construct MyVectorType from Eigen expressions
    template <typename Other>
    AlignedArray(const eig::ArrayBase<Other>& o)
    : Base(o)
    { }

    template<typename Other>
    AlignedArray& operator=(const eig::ArrayBase <Other>& o)
    {
      this->Base::operator=(o);
      return *this;
    }
  };

  
  template <class Type, size_t Size>
  class alignas(detail::vector_align<Size>()) AlignedVector 
  : public eig::Vector<Type, Size> {
    using Base = eig::Vector<Type, Size>;

  public:
    AlignedVector() : Base() 
    { }

    // This constructor allows you to construct MyVectorType from Eigen expressions
    template <typename Other>
    AlignedVector(const eig::MatrixBase<Other>& o)
    : Base(o)
    { }

    template<typename Other>
    AlignedVector& operator=(const eig::MatrixBase <Other>& o)
    {
      this->Base::operator=(o);
      return *this;
    }
  };

  /* Define common aligned vector/matrix types */
  
  using AlignedVector2ui = AlignedVector<uint, 2>;
  using AlignedVector3ui = AlignedVector<uint, 3>;
  using AlignedVector4ui = AlignedVector<uint, 4>;

  using AlignedVector2i = AlignedVector<int, 2>;
  using AlignedVector3i = AlignedVector<int, 3>;
  using AlignedVector4i = AlignedVector<int, 4>;

  using AlignedVector2f = AlignedVector<float, 2>;
  using AlignedVector3f = AlignedVector<float, 3>;
  using AlignedVector4f = AlignedVector<float, 4>;

  using AlignedMatrix2f = AlignedMatrix<float, 2, 2>;
  using AlignedMatrix3f = AlignedMatrix<float, 3, 3>;
  using AlignedMatrix4f = AlignedMatrix<float, 4, 4>;
};
