#pragma once

#include <metameric/core/detail/eigen.hpp>
#include <fmt/core.h>
#include <fmt/ranges.h>

namespace met {
  // Shorthand unsigned types
  using byte   = std::byte;
  using uint   = unsigned int;
  using uchar  = unsigned char;
  using ushort = unsigned short;

  struct Transform {
    eig::Vector3f position = 0.f; // Object central location
    eig::Vector3f rotation = 0.f; // X/Y/Z euler angles
    eig::Vector3f scaling  = 1.f;

  public:
    static Transform from_affine(const eig::Affine3f &aff) {
      Transform trf;

      // Obtain translation directly
      trf.position = aff.translation();
      
      // Separate rotation/scaling matrices
      eig::Matrix3f rot = eig::Matrix3f::Identity(), 
                    scl = eig::Matrix3f::Identity();
      aff.computeRotationScaling(&rot, &scl);

      // Obtain euler rotation and scaling from separated matrices
      trf.rotation.x() = std::acos((rot * eig::Vector3f::UnitX()).dot(eig::Vector3f::UnitX()));
      trf.rotation.y() = std::acos((rot * eig::Vector3f::UnitY()).dot(eig::Vector3f::UnitY()));
      trf.rotation.z() = std::acos((rot * eig::Vector3f::UnitZ()).dot(eig::Vector3f::UnitZ()));
      trf.scaling      = scl * eig::Vector3f(1.f);
            
      return trf;
    }

    eig::Affine3f affine() const {
      eig::Affine3f aff = eig::Affine3f::Identity();
      aff *= eig::Translation3f(position);
      aff *= eig::AngleAxisf(rotation.x(), eig::Vector3f::UnitX());
      aff *= eig::AngleAxisf(rotation.y(), eig::Vector3f::UnitY());
      aff *= eig::AngleAxisf(rotation.z(), eig::Vector3f::UnitZ());
      aff *= eig::Scaling(scaling.x(), scaling.y(), scaling.z());
      return aff;
    }

    auto operator==(const Transform &o) const {
      return position.isApprox(o.position) &&
             rotation.isApprox(o.rotation) &&
             scaling.isApprox(o.scaling);
    }
  };
} // namespace met

namespace Eigen {
  Affine3f lookat_rh(const Vector3f &eye,
                     const Vector3f &cen,
                     const Vector3f &up);

  Affine3f lookat_lh(const Vector3f &eye,
                     const Vector3f &cen,
                     const Vector3f &up);

  Projective3f ortho(float left, float right, float bottom, float top, float near, float far);

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

  inline
  Vector2u window_to_pixel(const Array2f &v,      // window-space vector
                           const Array2f &offs,   // window offset
                           const Array2f &size) { // window size
    auto v_ = (v - offs).cast<unsigned>().eval();
    return { v_.x(), size.y() - 1 - v_.y() };
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

  // Simple safe dividor in case some components may fall to 0
  inline
  Array4f safe_div(const Array4f &v, const Array4f &div) {
    return (v / div.NullaryExpr([](float f) { return f != 0.f ? f : 1.f; })).eval();
  }
} // namespace Eigen