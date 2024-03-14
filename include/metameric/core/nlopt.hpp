#pragma once

#include <metameric/core/math.hpp>
#include <metameric/core/utility.hpp>
#include <nlopt.hpp>
#include <functional>

namespace met {
  using NLOptAlgo = nlopt::algorithm;
  
  // NLOpt optimization direction; shorthand for negated objective function
  enum class NLOptForm {
    eMinimize, // Minimize objective function
    eMaximize  // Maximize objective function by minimizing negative
  };

  template <uint N>
  struct NLOptInfoT {
    using vec = eig::Vector<double, N>;
    using mat = eig::Matrix<double, N, -1>;

    struct Constraint {
      using Capture = std::function<double (eig::Map<const vec>, eig::Map<vec>)>;

    public:
      Capture f;
      double  tol = 0.0;
    };

    struct ConstraintV {
      using Capture = std::function<void (eig::Map<eig::VectorXd> result, eig::Map<const vec>, eig::Map<mat>)>;
      
    public:
      Capture f;
      uint    n   = 1;
      double  tol = 0.0;
    };

  public:
    NLOptAlgo algo = NLOptAlgo::LD_SLSQP;  // Employed algorithm
    NLOptForm form = NLOptForm::eMinimize; // Minimize/maximize?
    
    // Function arguments
    Constraint::Capture      objective;      // Minimization/maximization objective
    std::vector<Constraint>  eq_constraints; // Equality constraints:  f(x) == 0
    std::vector<Constraint>  nq_constraints; // Inequality constraint: f(x) <= 0
    std::vector<ConstraintV> eq_constraints_v; // Equality vector constraints:  f(x) == 0
    std::vector<ConstraintV> nq_constraints_v; // Inequality vector constraint: f(x) <= 0
    
    // Vector arguments
    vec                x_init; // Initial best guess for x
    std::optional<vec> upper;  // Upper bounds to solution 
    std::optional<vec> lower;  // Lower bounds to solution
    
    // Miscellany
    std::optional<double> stopval;
    std::optional<uint>   max_iters;        
    std::optional<double> max_time;        
    std::optional<double> rel_xpar_tol; // 1e-4
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
    std::optional<double> rel_xpar_tol; // 1e-4
  };

  // Return value for solve(NLOptInfo)
  struct NLOptResult {
    eig::VectorXd x;         // Result value
    double        objective; // Last objective value
    nlopt::result code;      // Optional return codes; 1 == success
  };

  template <uint N>
  struct NLOptResultT {
    NLOptInfoT<N>::vec x;         // Result value
    double             objective; // Last objective value
    nlopt::result      code;      // Optional return codes; 1 == success
  };
  
  template <uint N>
  NLOptResultT<N> solve(NLOptInfoT<N> &info);

  // Generate program and run optimization
  NLOptResult solve(NLOptInfo &info);

  // Solver functions
  namespace detail {
    // Describes f(x) = ||(Ax - b)|| with corresponding gradient
    template <uint N>
    auto func_norm(const auto &Af, const auto &bf) -> NLOptInfoT<N>::Constraint::Capture {
      using vec = NLOptInfoT<N>::vec;

      return 
        [A = Af.cast<double>().eval(), b = bf.cast<double>().eval()]
        (eig::Map<const vec> x, eig::Map<vec> g) {
          // shorthands for Ax - b and ||(Ax - b)||
          auto diff = ((A * x).array() - b).matrix().eval();
          auto norm = diff.norm();

          // g(x) = A^T * (Ax - b) / ||(Ax - b)||
          if (g.data())
            g = (A.transpose() * (diff.array() / norm).matrix()).eval();

          // f(x) = ||(Ax - b)||
          return norm;
      };
    };

    // Describes f(x) = ||(Ax - b)||^2 with corresponding gradient
    template <uint N>
    auto func_squared_norm(const auto &Af, const auto &bf) -> NLOptInfoT<N>::Constraint::Capture {
      using vec = NLOptInfoT<N>::vec;
      return 
        [A = Af.cast<double>().eval(), b = bf.cast<double>().eval()]
        (eig::Map<const vec> x, eig::Map<vec> g) {
          // shorthand for Ax - b
          auto diff = ((A * x).array() - b).matrix().eval();

          // g(x) = 2A(Ax - b)
          if (g.data())
            g = 2.0 * A.transpose() * diff;

          // f(x) = ||(Ax - b)||^2
          return diff.squaredNorm();
      };
    };

    // Describes f(x) = ||(Ax - b)||^2 with corresponding gradient
    template <uint N>
    auto func_supremum_norm(const auto &Af, const auto &bf) -> NLOptInfo::Capture {
      using vec = NLOptInfoT<N>::vec;
      return 
        [A = Af.cast<double>().eval(), b = bf.cast<double>().eval()]
        (eig::Map<const vec> x, eig::Map<vec> g) {
          // shorthand for Ax - b
          auto diff = ((A * x).array() - b).matrix().eval();

          // Find index and value of maximum coefficient
          uint i;
          double ret = diff.maxCoeff(&i);

          // Set all other components to zero, store only max coeff
          diff = 0.0;
          diff[i] = std::copysign(1.0, ret);

          // g(x) = A^T * (Ax - b) / ||(Ax - b)||
          if (g.data())
            g = (A.transpose() * diff).eval();

          // f(x) = ||(Ax - b)||
          return ret;
      };
    };

    // Describes f(x) = a * x - b with corresponding gradient
    template <uint N>
    auto func_dot(const auto &af, const auto &bf) -> NLOptInfoT<N>::Constraint::Capture {
      using vec = NLOptInfoT<N>::vec;
      return 
        [a = af.cast<double>().eval(), b = static_cast<double>(bf)]
        (eig::Map<const vec> x, eig::Map<vec> g) {
          // g(x) = a
          if (g.data())
            g = a;

          // f(x) = ax - b
          return a.dot(x) - b;
      };
    };

    // Describes f(x) = a * x - b with corresponding gradient
    template <uint N>
    auto func_dot_v(const auto &Af, const auto &bf) -> NLOptInfoT<N>::ConstraintV::Capture {
      using vec = NLOptInfoT<N>::vec;
      using mat = NLOptInfoT<N>::mat;

      return 
        [A = Af.cast<double>().eval(), b = bf.cast<double>().eval()]
        (eig::Map<eig::VectorXd> r, eig::Map<const vec> x, eig::Map<mat> g) {
          // g(x) = a
          if (g.data())
            g = A.transpose();

          // f(x) = ax - b
          r = (A * x).array() - b;
      };
    };
  } // namespace detail
} // namespace met