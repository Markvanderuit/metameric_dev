#pragma once

#include <Eigen/Dense>
#include <fmt/format.h>

namespace metameric {

/**
 * Map most common vector/matrix types from Eigen into program namespace
 */

namespace eig = Eigen;

using eig::Array2i;
using eig::ArrayXi;

using eig::Vector2i;
using eig::Vector3i;
using eig::Vector4i;
using eig::VectorXi;

using eig::Array2f;
using eig::ArrayXf;

using eig::Vector2f;
using eig::Vector3f;
using eig::Vector4f;
using eig::VectorXf;

using eig::Array22i;
using eig::Array33i;
using eig::Array44i;
using eig::ArrayXXi;

using eig::Matrix2i;
using eig::Matrix3i;
using eig::Matrix4i;
using eig::MatrixXi;

using eig::Array22f;
using eig::Array33f;
using eig::Array44f;
using eig::ArrayXXf;

using eig::Matrix2f;
using eig::Matrix3f;
using eig::Matrix4f;
using eig::MatrixXf;


/**
 * Custom formatter for Eigen's matrix/array types
 */

// template <> struct fmt::formatter<>

} // namespace metameric