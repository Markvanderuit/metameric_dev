#pragma once

#include <metameric/core/metamer.hpp>
#include <metameric/core/spectrum.hpp>
#include <metameric/core/utility.hpp>
#include <nlopt.hpp>
#include <functional>
#include <unordered_set>

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
    NLOptAlgo algo = NLOptAlgo::LD_SLSQP;  // Employed algorithm
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
    std::optional<double> max_time;        
    std::optional<double> rel_func_tol; // 1e-6;
    std::optional<double> rel_xpar_tol; // 1e-4
  };

  struct NLOptResult {
    eig::VectorXd x;
    double        objective;
    nlopt::result code;
  };

  NLOptResult solve(NLOptInfo &info);
  
  using NLMMVBoundarySet = typename std::unordered_set<
    Colr, eig::detail::matrix_hash_t<Colr>, eig::detail::matrix_equal_t<Colr>
  >;

  /* Info struct for sampling-based generation of points on the object color solid of a metamer mismatch volume */
  struct NLGenerateMMVBoundaryInfo {
    const Basis                  &basis;      // Spectral basis functions
    std::span<const CMFS>         systems_i;  // Color system spectra for prior color signals
    std::span<const Colr>         signals_i;  // Color signals for prior constraints
    std::span<const CMFS>         systems_j;  // Color system spectra for objective function
    const CMFS                    &system_j;  // Color system for mismatching region
    std::span<const eig::ArrayXf> samples;    // Random unit vector samples in (systems_i.size() + 1) * 3 dimensions
  };

  Spec              nl_generate_spectrum(GenerateSpectrumInfo info);
  std::vector<Spec> nl_generate_ocs_boundary_spec(const GenerateOCSBoundaryInfo &info);
  std::vector<Colr> nl_generate_ocs_boundary_colr(const GenerateOCSBoundaryInfo &info);
  std::vector<Spec> nl_generate_mmv_boundary_spec(const NLGenerateMMVBoundaryInfo &info, double power, bool switch_power);
  NLMMVBoundarySet  nl_generate_mmv_boundary_colr(const NLGenerateMMVBoundaryInfo &info, double power, bool switch_power);
} // namespace met