#pragma once

#include <metameric/core/detail/eigen.hpp>

namespace met {
  // Shorthand unsigned types
  using uint   = unsigned int;
  using ushort = unsigned short;
} // namespace met

namespace Eigen {
  Affine3f lookat_rh(const Vector3f &eye,
                     const Vector3f &cen,
                     const Vector3f &up);

  Affine3f lookat_lh(const Vector3f &eye,
                     const Vector3f &cen,
                     const Vector3f &up);

  Projective3f perspective_rh_no(float fovy, float aspect, float near, float far);
} // namespace Eigen