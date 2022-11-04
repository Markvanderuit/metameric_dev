#pragma once

#include <metameric/core/math.hpp>
#include <metameric/core/pca.hpp>
#include <metameric/core/spectrum.hpp>
#include <limits>
#include <vector>

namespace met {
  constexpr double lp_max_value = std::numeric_limits<double>::max();
  constexpr double lp_min_value = std::numeric_limits<double>::min();

  enum class LPMethod {
    ePrimal, // Primal simplex
    eDual    // Dual simplex
  };
  
  enum class LPCompare {
    eEQ,
    eLE,
    eGE
  };

  // Full set of parameters for a linear program
  struct LPParameters {
    LPMethod method = LPMethod::ePrimal;

    // Rows and cols
    uint M, N;

    // Components for defining "min C^T x + c0, w.r.t. Ax <=> b"
    eig::ArrayXd C;        /* N x 1 */
    eig::ArrayXd A;        /* M x N */
    eig::ArrayXd b;        /* M x 1 */

    // Relation for Ax <=> b (<=, ==, >=) 
    eig::ArrayX<LPCompare> r;

    // Bounds for the solution vector x; x_l <= x <= x_u
    eig::ArrayXd x_l, x_u; /* N x 1 */
  };

  // Comparative relationship operands for a linear program
  enum class LPComp : int {
    eEQ = 0, // ==
    eLE =-1, // <=
    eGE = 1  // >=
  };

  // Full set of parameters for a linear program
  template <typename Ty, uint N, uint M>
  struct LPParams {
    // Components for defining "min C^T x + c0, w.r.t. Ax <=> b"
    eig::Array<Ty, N, 1> C;
    eig::Array<Ty, M, N> A;
    eig::Array<Ty, M, 1> b;
    Ty                   c0;

    // Relation for Ax <=> b (<=, ==, >=)
    eig::Array<LPComp, M, 1> r = LPComp::eEQ;

    // Upper/lower limits to solution x
    eig::Array<Ty, N, 1> l = std::numeric_limits<Ty>::min();
    eig::Array<Ty, N, 1> u = std::numeric_limits<Ty>::max();
  };

  template <typename Ty>
  struct LPParamsX {
    uint N, M;

    // Components for defining "min C^T x + c0, w.r.t. Ax <=> b"
    eig::ArrayX<Ty>  C;  /* N x 1 */
    eig::MatrixX<Ty> A;  /* M x N */
    eig::ArrayX<Ty>  b;  /* M x 1 */
    Ty               c0; /* 1 x 1 */

    // Relation for Ax <=> b (<=, ==, >=) 
    eig::ArrayX<LPComp> r; /* M x 1 */

    // Upper/lower limits to solution x
    eig::ArrayX<Ty> l; /* N x 1 */
    eig::ArrayX<Ty> u; /* N x 1 */
  };

  // Solve a linear program given a valid parameter object
  eig::ArrayXd lp_solve(const LPParameters &params);

  // Solve a linear program using a params object
  template <typename Ty, uint N, uint M>
  eig::Matrix<Ty, N, 1> linprog(LPParams<Ty, N, M> &params);
  template <typename Ty>
  eig::MatrixX<Ty>      linprog(LPParamsX<Ty>      &params);
  template <typename Ty>
  eig::MatrixX<Ty>      linprog_test(LPParamsX<Ty> &params);
} // namespace met