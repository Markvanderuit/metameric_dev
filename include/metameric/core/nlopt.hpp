#pragma once

#include <metameric/core/metamer.hpp>
#include <metameric/core/spectrum.hpp>
#include <metameric/core/utility.hpp>
#include <nlopt.hpp>
#include <functional>
#include <unordered_set>

namespace met {
  using NLOptAlgo = nlopt::algorithm;
  
  // NLOpt optimization direction; shorthand for negated objective function
  enum class NLOptForm {
    eMinimize, // Minimize objective function
    eMaximize  // Maximize objective function by minimizing negative
  };

  struct NLOptInfo {
    using Capture = std::function<
      double (eig::Map<const eig::VectorXd>, eig::Map<eig::VectorXd>)
    >;

    using VectorCapture = std::function<
      void (eig::Map<eig::VectorXd> result, eig::Map<const eig::VectorXd>, eig::Map<eig::MatrixXd>)
    >;

    struct Constraint {
      Capture f;
      double  tol = 0.0;
    };

    struct VectorConstraint {
      VectorCapture f;
      uint          n   = 1;
      double        tol = 0.0;
    };

    uint      n;                           // Output dimensionality
    NLOptAlgo algo = NLOptAlgo::LD_SLSQP;  // Employed algorithm
    NLOptForm form = NLOptForm::eMinimize; // Minimize/maximize?

    // Function arguments
    Capture                       objective;      // Minimization/maximization objective
    std::vector<Constraint>       eq_constraints; // Equality constraints:  f(x) == 0
    std::vector<Constraint>       nq_constraints; // Inequality constraint: f(x) <= 0
    std::vector<VectorConstraint> eq_constraints_v; // Equality vector constraints:  f(x) == 0
    std::vector<VectorConstraint> nq_constraints_v; // Inequality vector constraint: f(x) <= 0

    // Vector arguments
    eig::VectorXd x_init; // Initial best guess for x
    eig::VectorXd upper;  // Upper bounds to solution 
    eig::VectorXd lower;  // Lower bounds to solution

    // Miscellany
    std::optional<double> stopval;
    std::optional<uint>   max_iters;        
    std::optional<double> max_time;        
    std::optional<double> rel_func_tol; // 1e-6
    std::optional<double> rel_xpar_tol; // 1e-4
  };

  // Return value for solve(NLOptInfo)
  struct NLOptResult {
    eig::VectorXd x;         // Result value
    double        objective; // Last objective value
    nlopt::result code;      // Optional return codes; 1 == success
  };

  // Generate program and run optimization
  NLOptResult solve(NLOptInfo &info);
} // namespace met