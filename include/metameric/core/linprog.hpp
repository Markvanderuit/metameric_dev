#pragma once

#include <metameric/core/math.hpp>
#include <metameric/core/spectrum.hpp>
#include <limits>
#include <vector>

namespace met {
  enum class LPMethod {
    ePrimal, // Primal simplex
    eDual    // Dual simplex
  };

  enum class LPObjective {
    eMinimize, // Minimize objective function
    eMaximize  // Maximize objective function by minimizing negative
  };

  enum class LPCompare : int {
    eLE =-1, 
    eEQ = 0, 
    eGE = 1
  };

  enum class LPStatus {
    eOptimal,
    ePrimalInfeasible,
    eDualInfeasible,
    eIterationHalted,
    eItBrokeCompletely
  };

  struct LPResult {
    LPStatus status;

    eig::ArrayXd x;
  };

  // Full set of parameters for a linear program
  struct LPParameters {
    // Constructors
    LPParameters() = default;
    LPParameters(uint M, uint N);

    // Method settings
    LPMethod    method    = LPMethod::ePrimal;
    LPObjective objective = LPObjective::eMinimize;
    bool        scaling   = true;

    // Rows and cols
    uint M, N;

    // Components for defining "min C^T x + c0, w.r.t. Ax = b"
    eig::ArrayXd  C;          /* N x 1 */
    eig::MatrixXd A;          /* M x N */
    eig::ArrayXd  b;          /* M x 1 */

    // Relation for Ax <=> b (<=, ==, >=) 
    eig::ArrayX<LPCompare> r; /* M x 1 */

    // Bounds for the solution vector x; x_l <= x <= x_u
    eig::ArrayXd x_l, x_u;    /* N x 1 */
  };

  // Solve a linear program for a valid parameter object
  eig::ArrayXd lp_solve(const LPParameters &params);
  std::pair<bool, eig::ArrayXd> lp_solve_res(const LPParameters &params);
} // namespace metw