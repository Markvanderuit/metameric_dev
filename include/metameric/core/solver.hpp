// Copyright (C) 2024 Mark van de Ruit, Delft University of Technology.
// 
// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.
// 
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
// 
// You should have received a copy of the GNU General Public License
// along with this program.  If not, see <https://www.gnu.org/licenses/>.

#pragma once

#include <metameric/core/fwd.hpp>
#include <metameric/core/utility.hpp>
#include <nlopt.hpp>
#include <autodiff/forward/real.hpp>
#include <autodiff/forward/real/eigen.hpp>
#include <functional>

namespace nlopt {
  // NLOpt optimization direction; shorthand for negated objective function
  enum class direction { eMinimize, eMaximize };

  // NLOpt problem description wrapper
  template <met::uint N>
  struct Wrapper {
    using vec = Eigen::Vector<double, N>;
    using mat = Eigen::Matrix<double, N, -1>;

    struct Constraint {
      using Capture = std::function<double (Eigen::Map<const vec>, Eigen::Map<vec>)>;
    public:
      Capture f;
      double  tol = 0.0;
    }; 

    struct ConstraintV {
      using Capture = std::function<void (Eigen::Map<Eigen::VectorXd> result, 
                                          Eigen::Map<const vec>, Eigen::Map<mat>)>;
    public:
      Capture   f;
      met::uint n   = 1;
      double    tol = 0.0;
    };

  public:
    algorithm algo = algorithm::LD_SLSQP;  // Employed algorithm
    direction dirc = direction::eMinimize; // Minimize/maximize?
    
    // Function arguments
    Constraint::Capture      objective;        // Minimization/maximization objective
    std::vector<Constraint>  eq_constraints;   // Equality constraints:  f(x) == 0
    std::vector<Constraint>  nq_constraints;   // Inequality constraint: f(x) <= 0
    std::vector<ConstraintV> eq_constraints_v; // Equality vector constraints:  f(x) == 0
    std::vector<ConstraintV> nq_constraints_v; // Inequality vector constraint: f(x) <= 0
    
    // Vector arguments
    vec                x_init; // Initial best guess for x
    std::optional<vec> upper;  // Upper bounds to solution 
    std::optional<vec> lower;  // Lower bounds to solution
    
    // Miscellany
    std::optional<double>    stopval;
    std::optional<met::uint> max_iters;        
    std::optional<double>    max_time;        
    std::optional<double>    rel_xpar_tol; // 1e-4
  };

  // NLOpt result description is std::pair<vector, code>
  template <met::uint N>
  using Result = std::pair<
    typename Wrapper<N>::vec, // Result value
    nlopt::result             // Optional NLOPT return code; 1 == success
  >;

  // Given a problem description, solve and generate a result 
  template <met::uint N>
  Result<N> solve(Wrapper<N> &info);

  /* Solver functions follow */
  
  // Describes f(x) = ||(Ax - b)|| with corresponding gradient
  template <met::uint N>
  auto func_norm(const auto &Af, const auto &bf) -> Wrapper<N>::Constraint::Capture {
    using namespace met;
    using vec = Wrapper<N>::vec;
    return 
      [A = Af.template cast<double>().eval(), b = bf.template cast<double>().eval()]
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
  template <met::uint N>
  auto func_squared_norm(const auto &Af, const auto &bf) -> Wrapper<N>::Constraint::Capture {
    using namespace met;
    using vec = Wrapper<N>::vec;
    return 
      [A = Af.template cast<double>().eval(), b = bf.template cast<double>().eval()]
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
  template <met::uint N>
  auto func_squared_norm_c(const auto &Af, const auto &bf, unsigned &iter) -> Wrapper<N>::Constraint::Capture {
    using namespace met;
    using vec = Wrapper<N>::vec;
    return 
      [A = Af.template cast<double>().eval(), b = bf.template cast<double>().eval(), &iter]
      (eig::Map<const vec> x, eig::Map<vec> g) {
        iter++;

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
  template <met::uint N>
  auto func_supremum_norm(const auto &Af, const auto &bf) -> Wrapper<N>::Capture {
    using namespace met;
    using vec = Wrapper<N>::vec;
    return 
      [A = Af.template cast<double>().eval(), b = bf.template cast<double>().eval()]
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
  template <met::uint N>
  auto func_dot(const auto &af, const auto &bf) -> Wrapper<N>::Constraint::Capture {
    using namespace met;
    using vec = Wrapper<N>::vec;
    return 
      [a = af.template cast<double>().eval(), b = static_cast<double>(bf)]
      (eig::Map<const vec> x, eig::Map<vec> g) {
        // g(x) = a
        if (g.data())
          g = a;

        // f(x) = ax - b
        return a.dot(x) - b;
    };
  };
  
  // Describes f(x) = a * x - b with corresponding gradient
  template <met::uint N>
  auto func_dot_v(const auto &Af, const auto &bf) -> Wrapper<N>::ConstraintV::Capture {
    using namespace met;
    using vec = Wrapper<N>::vec;
    using mat = Wrapper<N>::mat;
    return 
      [A = Af.template cast<double>().eval(), b = bf.template cast<double>().eval()]
      (eig::Map<eig::VectorXd> r, eig::Map<const vec> x, eig::Map<mat> g) {
        // g(x) = a
        if (g.data())
          g = A.transpose();

        // f(x) = ax - b
        r = (A * x).array() - b;
    };
  };
} // namespace nlopt

namespace autodiff {
  template <met::uint N>
  nlopt::Wrapper<N>::Constraint::Capture wrap_capture(std::function<real1st (const Eigen::Vector<real1st, N> &)> f) {
    using vec    = Eigen::Vector<double, N>;
    using ad_vec = Eigen::Vector<real1st, N>;
    return [f](Eigen::Map<const vec> x_, Eigen::Map<vec> g) -> double {
      ad_vec x = x_;
      if (g.data()) {
        real u;
        g = gradient(f, wrt(x), at(x), u);
        return u.val();
      } else {
        return f(x).val();
      }
    };
  }
} // namespace autodiff

// Introduce nlopt and autodiff namespace shorthands into metameric's namespace
namespace met {
  namespace opt = nlopt;
  namespace ad  = autodiff;
} // namespace met