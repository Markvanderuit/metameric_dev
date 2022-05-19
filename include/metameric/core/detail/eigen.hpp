#pragma once

#include <metameric/core/fwd.hpp>
#include <Eigen/Dense>

// TODO remove
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtx/string_cast.hpp>
#include <glm/gtc/type_ptr.hpp>

namespace met {
  using Array1ui = Eigen::Array<uint, 1, 1>;
  using Array2ui = Eigen::Array<uint, 2, 1>;
  using Array3ui = Eigen::Array<uint, 3, 1>;
  using Array4ui = Eigen::Array<uint, 4, 1>;
  using ArrayXui = Eigen::Array<uint, -1, 1>;

  using Vector1ui = Eigen::Matrix<uint, 1, 1>;
  using Vector2ui = Eigen::Matrix<uint, 2, 1>;
  using Vector3ui = Eigen::Matrix<uint, 3, 1>;
  using Vector4ui = Eigen::Matrix<uint, 4, 1>;
  using VectorXui = Eigen::Matrix<uint, -1, 1>;

  using Array1i = Eigen::Array<int, 1, 1>;
  using Eigen::Array2i;
  using Eigen::Array3i;
  using Eigen::Array4i;
  using Eigen::ArrayXi;

  using Eigen::Vector2i;
  using Eigen::Vector3i;
  using Eigen::Vector4i;
  using Eigen::VectorXi;

  using Array1f = Eigen::Array<float, 1, 1>;
  using Eigen::Array2f;
  using Eigen::Array3f;
  using Eigen::Array4f;
  using Eigen::ArrayXf;

  using Eigen::Vector2f;
  using Eigen::Vector3f;
  using Eigen::Vector4f;
  using Eigen::VectorXf;

  using Eigen::Array22i;
  using Eigen::Array33i;
  using Eigen::Array44i;
  using Eigen::ArrayXXi;

  using Eigen::Matrix2i;
  using Eigen::Matrix3i;
  using Eigen::Matrix4i;
  using Eigen::MatrixXi;

  using Eigen::Array22f;
  using Eigen::Array33f;
  using Eigen::Array44f;
  using Eigen::ArrayXXf;

  using Eigen::Matrix2f;
  using Eigen::Matrix3f;
  using Eigen::Matrix4f;
  using Eigen::MatrixXf;

  namespace math {
    inline
    Matrix4f orthogonal_matrix(float left, 
                               float right, 
                               float bottom,
                               float top,
                               float z_near,
                               float z_far) {
      Matrix4f m = Matrix4f::Identity();

      m(0, 0) = 2.f / (right - left);
      m(1, 1) = 2.f / (top - bottom);
      m(2, 2) = - 2.f / (z_far - z_near);
      m(3, 0) = - (right + left) / (right - left);
      m(3, 1) = - (top + bottom) / (top - bottom);
      m(3, 2) = - (z_far + z_near) / (z_far - z_near);

      return m;
    }

    inline
    Matrix4f perspective_matrix(float fov_y, 
                                float aspect,
                                float z_near,
                                float z_far) {
      const float tan_half_fov_y = std::tanf(fov_y / 2.f);

      Matrix4f m = Matrix4f::Zero();

      m(0, 0) = 1.f / (aspect * tan_half_fov_y);
      m(1, 1) = 1.f / tan_half_fov_y;
      m(2, 2) = - (z_far + z_near) / (z_far - z_near);
      m(2, 3) = - 1.f;
      m(3, 2) = - (2.f * z_far * z_near) / (z_far - z_near);

      return m.transpose();
    }

    inline
    Matrix4f lookat_matrix(const Vector3f &eye,
                           const Vector3f &center,
                           const Vector3f &up) {
      const auto f = (center.transpose() - eye.transpose()).normalized();
      const auto s = f.cross(up.transpose()).normalized();
      const auto u = s.cross(f);

      Matrix4f m;

      m << s, 0,
           u, 0,
          -f, 0,
          -s.dot(eye), -u.dot(eye), f.dot(eye), 1;

      return m.transpose();
    }
  } // namespace math
} // namespace met