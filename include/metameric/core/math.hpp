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
  
  // Convert a screen-space vector in [0, 1] to world space
  inline
  Vector3f screen_to_world_space(const Vector2f      &v,
                                 const Projective3f  &mat) {
    Array2f v_ = (v.array() - 0.5f) * 2.f;
    Array4f trf = mat.inverse() * (Vector4f() << v_, 0, 1).finished();
    return trf.head<3>() / trf[3];
  }

  inline
  Vector2f window_to_screen_space(const Array2f &v,      // window-space vector
                                  const Array2f &offs,   // window offset
                                  const Array2f &size) { // window size
    auto v_ = ((v - offs) / size).eval();
    return { v_.x(), 1.f - v_.y() };
  }

  // Convert a world-space vector to screen space in [0, 1]
  inline
  Vector2f world_to_screen_space(const Vector3f     &v,     // world-space vector
                                 const Projective3f &mat) { // camera view/proj matrix
    Array4f trf = mat * (Vector4f() << v, 1).finished();
    return trf.head<2>() / trf.w() * .5f + .5f;
  }

  // Convert a screen-space vector in [0, 1] to window space
  inline
  Vector2f screen_to_window_space(const Array2f &v,      // screen-space vector
                                  const Array2f &offs,   // window offset
                                  const Array2f &size) { // window size
    return offs + size * Array2f(v.x(), 1.f - v.y());
  }

  // Convert a world-space vector to window space
  inline
  Vector2f world_to_window_space(const Vector3f     &v,      // world-space vector
                                 const Projective3f &mat,    // camera view/proj matrix
                                 const Vector2f     &offs,   // window offset
                                 const Vector2f     &size) { // window size
    return screen_to_window_space(world_to_screen_space(v, mat), offs, size);
  }
} // namespace Eigen