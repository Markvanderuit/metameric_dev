#include <metameric/components/pipeline/detail/bvh.hpp>

namespace met::detail {
  

  template <uint D, BVHPrimitive Ty>
  BVH<D, Ty>::BVH(std::span<const eig::Array3f> vt)
  requires(Ty == BVHPrimitive::ePoint) {
    // ...
  }

  template <uint D, BVHPrimitive Ty>
  BVH<D, Ty>::BVH(std::span<const eig::Array3f> vt, std::span<const eig::Array3u> el)
  requires(Ty == BVHPrimitive::eTriangle) {
    // ...
  }

  template <uint D, BVHPrimitive Ty>
  BVH<D, Ty>::BVH(std::span<const eig::Array3f> vt, std::span<const eig::Array4u> el)
  requires(Ty == BVHPrimitive::eTetrahedron) {
    // ...
  }
} // namespace met::detail