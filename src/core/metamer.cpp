#include <metameric/core/nlopt.hpp>
#include <metameric/core/metamer.hpp>
#include <metameric/core/ranges.hpp>
#include <metameric/core/utility.hpp>
#include <oneapi/tbb/concurrent_vector.h>
#include <algorithm>
#include <execution>
#include <unordered_set>

namespace met {
  namespace detail {
    // Describes f(x) = ||(Ax - b)|| with corresponding gradient
    auto func_norm(const auto &Af, const auto &bf) -> NLOptInfo::Capture {
      return 
        [A = Af.cast<double>().eval(), b = bf.cast<double>().eval()]
        (eig::Map<const eig::VectorXd> x, eig::Map<eig::VectorXd> g) {
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
    auto func_squared_norm(const auto &Af, const auto &bf) -> NLOptInfo::Capture {
      return 
        [A = Af.cast<double>().eval(), b = bf.cast<double>().eval()]
        (eig::Map<const eig::VectorXd> x, eig::Map<eig::VectorXd> g) {
          // shorthand for Ax - b
          auto diff = ((A * x).array() - b).matrix().eval();

          // g(x) = 2A(Ax - b)
          if (g.data())
            g = 2.0 * A.transpose() * diff;

          // f(x) = ||(Ax - b)||^2
          return diff.squaredNorm();
      };
    };

    // Describes f(x) = a * x - b with corresponding gradient
    auto func_dot(const auto &af, const auto &bf) -> NLOptInfo::Capture {
      return 
        [a = af.cast<double>().eval(), b = static_cast<double>(bf)]
        (eig::Map<const eig::VectorXd> x, eig::Map<eig::VectorXd> g) {
          // g(x) = a
          if (g.data())
            g = a;

          // f(x) = ax - b
          return a.dot(x) - b;
      };
    };
  } // namespace detail

  Spec generate_spectrum(GenerateSpectrumInfo info) {
    met_trace();
    debug::check_expr(info.systems.size() == info.signals.size(),
      "Color system size not equal to color signal size");

    // Solver settings
    NLOptInfo solver = {
      .n            = wavelength_bases,
      .algo         = NLOptAlgo::LD_SLSQP,
      .form         = NLOptForm::eMinimize,
      .x_init       = Basis::BVec(0.5).cast<double>().eval(),
      .max_iters    = 128,  // Failsafe; halt after 128 iterations
      // .rel_func_tol = 1e-6,
      .rel_xpar_tol = 1e-3, // halt when constraint error falls below threshold
    };
    
    // Note; simple way to get relatively smooth spectra
    // Objective function minimizes l2 norm over spectral distribution itself
    solver.objective = detail::func_squared_norm(info.basis.func, Spec::Zero());

    // Add color system equality constraints, upholding spectral metamerism
    for (uint i = 0; i < info.systems.size(); ++i) {
      auto A = (info.systems[i].transpose() * info.basis.func.matrix()).eval();
      auto o = (info.systems[i].transpose() * info.basis.mean.matrix()).eval();
      auto b = (info.signals[i] - o.array()).eval();
      solver.eq_constraints.push_back(detail::func_squared_norm(A, b));
    } // for (uint i)

    // Add boundary inequality constraints, upholding spectral 0 <= x <= 1
    for (uint i = 0; i < wavelength_samples; ++i) {
      auto  a  = info.basis.func.row(i).eval();
      float ub = 1.f - info.basis.mean[i];
      float lb = ub - 1.f;
      solver.nq_constraints.push_back(detail::func_dot( a,  ub));
      solver.nq_constraints.push_back(detail::func_dot(-a, -lb));
    } // for (uint i)

    // Run solver and return recovered spectral distribution
    NLOptResult r = solve(solver);
    return (info.basis.func * r.x.cast<float>()).array() + info.basis.mean;
  }

  Spec generate_spectrum(GenerateIndirectSpectrumInfo info) {
    met_trace();

    NLOptInfo solver = {
      .n            = wavelength_bases,
      .algo         = NLOptAlgo::LD_SLSQP,
      .form         = NLOptForm::eMinimize,
      .x_init       = Basis::BVec(0.5).cast<double>().eval(),
      .max_iters    = 1024,  // Failsafe; halt after 1024 iterations
      .rel_xpar_tol = 1e-3,
    };

    // Objective function minimizes l2 norm over spectral distribution
    solver.objective = detail::func_norm(info.basis.func, Spec::Zero());

    // Add boundary inequality constraints, upholding spectral 0 <= x <= 1
    for (uint i = 0; i < wavelength_samples; ++i) {
      auto  a  = info.basis.func.row(i).eval();
      float ub = 1.f; /* 1.f - info.basis.mean[i]; */
      float lb = ub - 1.f;
      solver.nq_constraints.push_back(detail::func_dot( a,  ub));
      solver.nq_constraints.push_back(detail::func_dot(-a, -lb));
    } // for (uint i)

    // Add color system equality constraints, upholding spectral surface metamerism
    {
      auto A = (info.base_system.transpose() * info.basis.func.matrix()).eval();
      // auto o = (info.base_system.transpose() * info.basis.mean.matrix()).eval();
      auto b = (info.base_signal /* - o.array() */).eval();
      solver.eq_constraints.push_back(detail::func_norm(A, b));
    } // for (uint i)
    
    // Add interreflection equality constraint, upholding requested output color;
    // specify three equalities for three partial derivatives
    for (uint j = 0; j < 3; ++j) {
      solver.eq_constraints.push_back(
        [A = info.refl_systems
           | vws::transform([j](const CMFS &cmfs) { 
              return cmfs.col(j).transpose().cast<double>().eval(); })
           | rng::to<std::vector>(), 
         B = info.basis.func.matrix().cast<double>().eval(), 
         b = static_cast<double>(info.refl_signal[j] /* - mean_colr[j] */)]
        (eig::Map<const eig::VectorXd> x, eig::Map<eig::VectorXd> g) {
          // Recover spectral distribution
          auto r = (B * x).eval();
          
          // f(x) = ||Ax - b||
          // -> a_0 + a_1*(Bx) + a_2*(Bx)^2 + a_3*(Bx)^3 + ... - b
          double diff = A[0].sum();
          for (uint i = 1; i < A.size(); ++i) {
            double p = static_cast<double>(i);
            auto rp  = r.array().pow(p).matrix().eval();
            diff += A[i] * rp;
          }
          diff -= b;

          double norm = diff; // hoboy
          
          // g(x) = ||Ax - b||
          // -> (B^T*A_1*(Bx)^0 + B^T*2*A_2*(Bx) + B^T*3*a_3*(Bx)^2 + ...)
          eig::Vector<double, wavelength_bases> grad = 0.0;
          for (uint i = 1; i < A.size(); ++i) {
            double p = static_cast<double>(i);
            auto rp  = r.array().pow(p - 1.0).matrix().eval();
            grad += p
                  * B.transpose()
                  * A[i].transpose().cwiseProduct(rp);
          }

          if (g.data())
            g = grad;

          fmt::print("{}\n", norm);
          return norm;
      });
    } // for (uint j)

    // Run solver and return recovered spectral distribution
    NLOptResult r = solve(solver);
    Spec s = (info.basis.func * r.x.cast<float>()).array()/*  + info.basis.mean */;

    // TODO remove
    /* { // Debug time
      // Establish output of basis mean in nonlinear system
      Colr mean_colr = 0.f;
      // mean_colr = info.base_system.transpose() * info.basis.mean.matrix();
      for (uint i = 0; i < info.refl_systems.size(); ++i) {
        double p = static_cast<double>(i);
        auto rp  = info.basis.mean.pow(p).matrix().eval();
        mean_colr += (info.refl_systems[i].transpose() * rp).array();
      }

      Colr full_colr = 0.f;
      // full_colr = info.base_system.transpose() * s.matrix();
      for (uint i = 0; i < info.refl_systems.size(); ++i) {
        double p = static_cast<double>(i);
        auto rp  = s.pow(p).matrix().eval();
        full_colr += (info.refl_systems[i].transpose() * rp).array();
      }

      Spec remainder = s - info.basis.mean;      
      Colr remainder_colr = 0.f;
      // remainder_colr = info.base_system.transpose() * remainder.matrix();
      for (uint i = 0; i < info.refl_systems.size(); ++i) {
        double p = static_cast<double>(i);
        auto rp  = remainder.pow(p).matrix().eval();
        remainder_colr += (info.refl_systems[i].transpose() * rp).array();
      }

      Colr added_colr = mean_colr + remainder_colr;
      Colr error_colr = added_colr - full_colr;

      fmt::print("expected: {}\nmean: {}\nremainder: {}\nadded: {}\nerror: {}\n", full_colr, mean_colr, remainder_colr, added_colr, error_colr);
    } */
    
    return s;
  }

  std::vector<Spec> generate_ocs_boundary_spec(const GenerateOCSBoundaryInfo &info) {
    met_trace();

    // Run multithreaded spectrum generation
    std::vector<Spec> output(info.samples.size());
    std::transform(std::execution::par_unseq, range_iter(info.samples), output.begin(), [&](const Colr &sample) {
      // Obtain spectrum by projecting sample onto optimal
      Spec s = (info.system * sample.matrix()).eval();
      for (auto &f : s)
        f = f >= 0.f ? 1.f : 0.f;
      
      // Run solve to find nearest spectrum within the basis function set
      std::vector<CMFS> systems = { info.system };
      std::vector<Colr> signals = { (info.system.transpose() * s.matrix()).eval() };
      return generate_spectrum({  .basis = info.basis, .systems = systems, .signals = signals });
    });
    
    // Filter NaNs at underconstrained output and strip redundancy from return 
    std::erase_if(output, [](Spec &c) { return c.isNaN().any(); });
    std::unordered_set<
      Spec, 
      decltype(eig::detail::matrix_hash<float>), 
      decltype(eig::detail::matrix_equal)
    > output_unique(range_iter(output));
    return std::vector<Spec>(range_iter(output_unique));
  }

  std::vector<Colr> generate_ocs_boundary_colr(const GenerateOCSBoundaryInfo &info) {
    met_trace();

    std::vector<Spec> input = generate_ocs_boundary_spec(info);
    std::vector<Colr> output(input.size());

    std::transform(std::execution::par_unseq, range_iter(input), output.begin(), [&](const Spec &s) {
      return (info.system.transpose() * s.matrix()).eval();
    });
    
    // Filter NaNs at underconstrained output and strip redundancy from return 
    std::erase_if(output, [](Colr &c) { return c.isNaN().any(); });
    std::unordered_set<
      Colr, 
      decltype(Eigen::detail::matrix_hash<float>), 
      decltype(Eigen::detail::matrix_equal)
    > output_unique(range_iter(output));
    return std::vector<Colr>(range_iter(output_unique));
  }

  std::vector<Spec> generate_mmv_boundary_spec(const GenerateMMVBoundaryInfo &info) {
    met_trace();

    NLOptInfo solver = {
      .n            = wavelength_bases,
      .algo         = NLOptAlgo::LD_SLSQP,
      .form         = NLOptForm::eMaximize,
      .x_init       = Basis::BVec(0.5).cast<double>().eval(),
      .rel_func_tol = 1e-4,
      .rel_xpar_tol = 1e-2,
    };

    // Construct orthogonal matrix used during maximiation
    auto S = rng::fold_left_first(info.systems_j, std::plus<CMFS> {}).value().eval();
    eig::JacobiSVD<decltype(S)> svd;
    svd.compute(S, eig::ComputeFullV);
    auto U = (S * svd.matrixV() * svd.singularValues().asDiagonal().inverse()).eval();

    // Add color system equality constraints, upholding spectral metamerism
    for (uint i = 0; i < info.systems_i.size(); ++i) {
      auto A = (info.systems_i[i].transpose() * info.basis.func).eval();
      auto b = info.signals_i[i];
      solver.eq_constraints.push_back(detail::func_norm(A, b));
    } // for (uint i)

    // Add boundary inequality constraints, upholding spectral 0 <= x <= 1
    for (uint i = 0; i < wavelength_samples; ++i) {
      auto  a  = info.basis.func.row(i).eval();
      float ub = 1.f; // 1.f - info.basis.mean[i];
      float lb = ub - 1.f;
      solver.nq_constraints.push_back(detail::func_dot( a,  ub));
      solver.nq_constraints.push_back(detail::func_dot(-a, -lb));
    } // for (uint i)

    // Parallel solve for boundary spectra
    tbb::concurrent_vector<Spec> tbb_output;
    tbb_output.reserve(info.samples.size());
    #pragma omp parallel
    {
      // Per thread copy of current solver parameter set
      NLOptInfo local_solver = solver;

      #pragma omp for
      for (int i = 0; i < info.samples.size(); ++i) {
        // Define objective function: max (Uk)^T (Bx) -> max C^T Bx -> max ax
        auto a = ((U * info.samples[i].matrix()).transpose() * info.basis.func).transpose().eval();
        local_solver.objective = detail::func_dot(a, 0.f);

        // Run solver and store recovered spectral distribution if it is safe
        NLOptResult r = solve(local_solver);
        Spec s = info.basis.func * r.x.cast<float>();
        guard_continue(!s.isNaN().any());
        tbb_output.push_back(s.cwiseMin(1.f).cwiseMax(0.f).eval());
      } // for (int i)
    }

    return std::vector<Spec>(range_iter(tbb_output));
  }

  std::vector<Spec> generate_mmv_boundary_spec(const GenerateIndirectMMVBoundaryInfo &info) {
    met_trace();

    NLOptInfo solver = {
      .n            = wavelength_bases,
      .algo         = NLOptAlgo::LD_SLSQP,
      .form         = NLOptForm::eMaximize,
      .x_init       = Basis::BVec(0.5).cast<double>().eval(),
      .max_iters    = 1024,  // Failsafe; halt after 1024 iterations
      .rel_func_tol = 1e-4,
      .rel_xpar_tol = 1e-2,
    };

    // Add color system equality constraints, upholding spectral metamerism
    for (uint i = 0; i < info.systems_i.size(); ++i) {
      auto A = (info.systems_i[i].transpose() * info.basis.func.matrix()).eval();
      // auto o = (info.systems_i[i].transpose() * info.basis.mean.matrix()).eval();
      auto b = (info.signals_i[i] /* - o.array() */).eval();
      solver.eq_constraints.push_back(detail::func_norm(A, b));
    } // for (uint i)

    // Add boundary inequality constraints, upholding spectral 0 <= x <= 1
    for (uint i = 0; i < wavelength_samples; ++i) {
      auto  a  = info.basis.func.row(i).eval();
      float ub = 1.f /* - info.basis.mean[i] */;
      float lb = ub - 1.f;
      solver.nq_constraints.push_back(detail::func_dot( a,  ub));
      solver.nq_constraints.push_back(detail::func_dot(-a, -lb));
    } // for (uint i)

    // Parallel solve for boundary spectra
    tbb::concurrent_vector<Spec> tbb_output;
    tbb_output.reserve(info.samples.size());
    #pragma omp parallel
    {
      // Per thread copy of current solver parameter set
      NLOptInfo local_solver = solver;

      #pragma omp for
      for (int i = 0; i < info.samples.size(); ++i) {
        local_solver.objective = 
          [A = info.systems_j
             | vws::transform([s = info.samples[i]](const CMFS &cmfs) {
               return (cmfs * s.matrix()).transpose().cast<double>().eval();
             }) | rng::to<std::vector>(), 
           B = info.basis.func.cast<double>().eval()]
          (eig::Map<const eig::VectorXd> x, eig::Map<eig::VectorXd> g) -> double {
            // Recover spectral distribution
            auto r = (B * x).eval();

            auto diff =/*  0.0; */ A[0].sum();
            auto grad = eig::Vector<double, wavelength_bases>(0.0);
            for (uint i = 1; i < A.size(); ++i) {
              double p = static_cast<double>(i);
              auto fr  = r.array().pow(p).matrix().eval(); 
              auto dr  = r.array().pow(p - 1.0).matrix().eval();
              diff += A[i] * fr;
              grad += p
                    * B.transpose()
                    * A[i].transpose().cwiseProduct(dr);
            }

            if (g.data())
              g = grad;
            return diff;
        };

        // Run solver and store recovered spectral distribution if it is safe
        NLOptResult r = solve(local_solver);
        Spec s = info.basis.func * r.x.cast<float>();
        guard_continue(!s.isNaN().any());
        tbb_output.push_back(s.cwiseMin(1.f).cwiseMax(0.f).eval());
      } // for (int i)
    }

    return std::vector<Spec>(range_iter(tbb_output));
  }

  std::vector<Colr> generate_mmv_boundary_colr(const GenerateIndirectMMVBoundaryInfo &info) {
    met_trace();

    // Generate unique boundary spectra
    auto spectra = generate_mmv_boundary_spec(info);

    // Transform to non-unique colors
    std::vector<Colr> colors(spectra.size());
    std::transform(std::execution::par_unseq, 
      range_iter(spectra), colors.begin(),
      [&](const Spec &r) -> Colr { 
        // Compute output radiance based on system of powers
        Spec s = 0.f;
        for (uint i = 0; i < info.components.size(); ++i) {
          float p = static_cast<float>(i);
          s += info.components[i] * r.array().pow(p);
        }
        
        // Transform to output color
        return (info.system_j.transpose() * s.matrix()).eval(); 
      });

    // Collapse return value to unique colors
    std::unordered_set<
      Colr, 
      decltype(Eigen::detail::matrix_hash<float>), 
      decltype(Eigen::detail::matrix_equal)
    > output_unique(range_iter(colors));
    return std::vector<Colr>(range_iter(output_unique));
  }

  std::vector<Colr> generate_mmv_boundary_colr(const GenerateMMVBoundaryInfo &info) {
    met_trace();

    // Generate unique boundary spectra
    auto spectra = generate_mmv_boundary_spec(info);

    // Transform to non-unique colors
    std::vector<Colr> colors(spectra.size());
    std::transform(std::execution::par_unseq, range_iter(spectra), colors.begin(),
      [&](const auto &s) -> Colr {  return (info.system_j.transpose() * s.matrix()).eval(); });

    // Collapse return value to unique colors
    std::unordered_set<
      Colr, 
      decltype(Eigen::detail::matrix_hash<float>), 
      decltype(Eigen::detail::matrix_equal)
    > output_unique(range_iter(colors));
    return std::vector<Colr>(range_iter(output_unique));
  }
} // namespace met