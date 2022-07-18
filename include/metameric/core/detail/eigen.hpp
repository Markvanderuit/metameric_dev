#pragma once

// Extensions to Eigen's existing classes are inserted through header files
#define EIGEN_ARRAYBASE_PLUGIN  "eigen_arraybase.ext"
#define EIGEN_MATRIXBASE_PLUGIN "eigen_matrixbase.ext"
#define EIGEN_ARRAY_PLUGIN      "eigen_array.ext"
#define EIGEN_MATRIX_PLUGIN     "eigen_matrix.ext"

#include <Eigen/Dense>

namespace met {
  namespace eig = Eigen; // namespace shorthand
} // namespace met

namespace Eigen {
  namespace detail {
    template <size_t D>
    constexpr size_t vector_align() {
      static_assert(D > 0 && D <= 4);
      return D == 4 ? 16
           : D == 3 ? 16
           : D == 2 ? 8
           : 4;
    }

    template <size_t D>
    constexpr size_t _matrix_align() {
      static_assert(D > 0 && D <= 4);
      return D == 4 ? 4
           : D == 3 ? 4
           : D == 2 ? 2
           : 1;
    }

    template <size_t R, size_t C>
    constexpr size_t matrix_align() {
      return _matrix_align<R>() * vector_align<C>();
    }
  } // namespace detail

  template <class Type, size_t Rows, size_t Cols>
  class alignas(detail::matrix_align<Rows, Cols>()) AlMatrix 
  : public Matrix<Type, Rows, Cols> {
    using Base = Matrix<Type, Rows, Cols>;

  public:
    AlMatrix() : Base() 
    { }

    // This constructor allows you to construct MyVectorType from Eigen expressions
    template <typename Other>
    AlMatrix(const MatrixBase<Other>& o)
    : Base(o)
    { }

    template<typename Other>
    AlMatrix& operator=(const MatrixBase <Other>& o)
    {
      this->Base::operator=(o);
      return *this;
    }
  };

  template <class Type, size_t Rows, size_t Cols>
  class alignas(detail::matrix_align<Rows, Cols>()) AlArray 
  : public Array<Type, Rows, Cols> {
    using Base = Array<Type, Rows, Cols>;

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

  /* Define common aligned vector/matrix types */
  
  using AlVector2f = AlVector<float, 2>;
  using AlVector3f = AlVector<float, 3>;
  using AlVector4f = AlVector<float, 4>;

  using AlMatrix2f = AlMatrix<float, 2, 2>;
  using AlMatrix3f = AlMatrix<float, 3, 3>;
  using AlMatrix4f = AlMatrix<float, 4, 4>;

  using AlArray2f = AlArray<float, 3, 1>;
  using AlArray3f = AlArray<float, 3, 1>;
  using AlArray4f = AlArray<float, 3, 1>;

  using AlArray22f = AlArray<float, 3, 2>;
  using AlArray33f = AlArray<float, 3, 3>;
  using AlArray44f = AlArray<float, 3, 4>;
} // namespace Eigen
