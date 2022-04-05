#pragma once

#include <Eigen/Dense>
#include <fmt/format.h>

namespace metameric {

/**
 * Map most common vector/matrix types from Eigen into program namespace
 */

namespace eig = Eigen;

using eig::Vector2i;
using eig::Vector3i;
using eig::Vector4i;

using eig::Vector2f;
using eig::Vector3f;
using eig::Vector4f;

using eig::Matrix2i;
using eig::Matrix3i;
using eig::Matrix4i;

using eig::Matrix2f;
using eig::Matrix3f;
using eig::Matrix4f;

using eig::VectorXi;
using eig::VectorXf;
using eig::MatrixXi;
using eig::MatrixXf;

/**
 * Custom formatter for Eigen's matrix/array types
 */

// template <> struct fmt::formatter<>

} // namespace metameric