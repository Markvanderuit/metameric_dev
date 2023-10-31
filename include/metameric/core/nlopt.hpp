#pragma once

#include <metameric/core/metamer.hpp>
#include <metameric/core/spectrum.hpp>
#include <functional>
#include <nlopt.hpp>

namespace met {
  using NLOptAlgo = nlopt::algorithm;
  
  enum class NLOptForm {
    eMinimize, // Minimize objective function
    eMaximize  // Maximize objective function by minimizing negative
  };

  struct NLOptInfo {
    using Capture = std::function<
      double (eig::Map<const eig::VectorXd>, eig::Map<eig::VectorXd>)
    >;

    uint      n;                           // Output dimensionality
    NLOptAlgo algo = NLOptAlgo::LD_MMA;    // Employed algorithm
    NLOptForm form = NLOptForm::eMinimize; // Minimize/maximize?

    // Function arguments
    Capture              objective;      // Minimization/maximization objective
    std::vector<Capture> eq_constraints; // Equality constraints:  f(x) == 0
    std::vector<Capture> nq_constraints; // Inequality constraint: f(x) <= 0

    // Vector arguments
    eig::VectorXd x_init; // Initial best guess for x
    eig::VectorXd upper;  // Upper bounds to solution 
    eig::VectorXd lower;  // Lower bounds to solution

    // Miscellany
    std::optional<double> stopval;
    std::optional<uint>   max_iters;
    double relative_func_tol = 1e-06;
    double relative_xpar_tol = 1e-03;
  };

  struct NLOptResult {
    eig::VectorXd x;
    double        objective;
    nlopt::result code;
  };

  NLOptResult solve(NLOptInfo &info);

  void test_nlopt();
  Spec nl_generate_spectrum(GenerateSpectrumInfo info);
} // namespace met