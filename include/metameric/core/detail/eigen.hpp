#pragma once

// Extensions to Eigen's existing classes are inserted through header files
#define EIGEN_ARRAYBASE_PLUGIN  "eigen_arraybase.ext"
#define EIGEN_MATRIXBASE_PLUGIN "eigen_matrixbase.ext"
#define EIGEN_ARRAY_PLUGIN      "eigen_array.ext"
#define EIGEN_MATRIX_PLUGIN     "eigen_matrix.ext"

#include <Eigen/Dense>

namespace met {
  namespace eig = Eigen; // namespace shorthand

  // Concept for detecting Eigen's isApprox(...) member function
  template <typename T>
  concept is_approx_comparable = requires(const T &a, const T &b) {
    { a.isApprox(b) } -> std::convertible_to<bool>;
  };
} // namespace met

namespace Eigen {
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

  /* Define useful unsigned types */
  
  using Array2u = Array<unsigned int, 2, 1>;
  using Array3u = Array<unsigned int, 3, 1>;
  using Array4u = Array<unsigned int, 4, 1>;

  using Vector2u = Matrix<unsigned int, 2, 1>;
  using Vector3u = Matrix<unsigned int, 3, 1>;
  using Vector4u = Matrix<unsigned int, 4, 1>;

  /* Define common aligned vector types */
  
  using AlVector2f = AlVector<float, 2>;
  using AlVector3f = AlVector<float, 3>;
  using AlVector4f = AlVector<float, 4>;

  using AlArray2f = AlArray<float, 2>;
  using AlArray3f = AlArray<float, 3>;
  using AlArray4f = AlArray<float, 4>;

  using AlArray2u = AlArray<unsigned int, 2>;
  using AlArray3u = AlArray<unsigned int, 3>;
  using AlArray4u = AlArray<unsigned int, 4>;
} // namespace Eigen
