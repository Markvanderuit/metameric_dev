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
  
  // Convert a world-space vector to screen space in [0, 1]
  inline
  Vector2f screen_space(const Vector3f     &v,       // world-space vector
                        const Projective3f &mat) {   // camera view/proj matrix
    Array4f trf = mat * (Vector4f() << v, 1).finished();
    return trf.head<2>() / trf.w() * .5f + .5f;
  }

  // Convert a screen-space vector in [0, 1] to window space
  inline
  Vector2f window_space(const Array2f &v,            // screen-space vector
                        const Array2f &offs,        // window offset
                        const Array2f &size) {      // window size
    return offs + size * Array2f(v.x(), 1.f - v.y());
  }

  // Convert a world-space vector to window space
  inline
  Vector2f window_space(const Vector3f     &v,       // world-space vector
                        const Projective3f &mat,    // camera view/proj matrix
                        const Vector2f     &offs,   // window offset
                        const Vector2f     &size) { // window size
    return window_space(screen_space(v, mat), offs, size);
  }
} // namespace Eigen