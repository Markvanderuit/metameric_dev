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

          // g(x) = A^T * (Ax - b) / ||(-(Ax - b))||
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

          // f(x) = a*x - b
          return a.dot(x) - b;
      };
    };
  } // namespace detail

  Spec generate_spectrum(GenerateSpectrumInfo info) {
    met_trace();
    debug::check_expr(info.systems.size() == info.signals.size(),
      "Color system size not equal to color signal size");

    NLOptInfo solver = {
      .n            = wavelength_bases,
      .algo         = NLOptAlgo::LD_SLSQP,
      .form         = NLOptForm::eMinimize,
      .x_init       = Basis::BVec(0.5).cast<double>().eval(),
      .rel_func_tol = 1e-4,
      .rel_xpar_tol = 1e-2,
    };

    // Objective function minimizes l2 norm over spectral distribution
    solver.objective = detail::func_norm(info.basis.func, Spec::Zero());

    // Add color system equality constraints, upholding spectral metamerism
    for (uint i = 0; i < info.systems.size(); ++i) {
      auto A = (info.systems[i].transpose() * info.basis.func.matrix()).eval();
      auto o = (info.systems[i].transpose() * info.basis.mean.matrix()).eval();
      auto b = (info.signals[i] - o.array()).eval();
      solver.eq_constraints.push_back(detail::func_norm(A, b));
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

    // Helper spectra
    using DCMFS = eig::Matrix<double, 3, wavelength_samples>;
    using DSpec = eig::Vector<double, wavelength_samples>;
    using BSpec = eig::Vector<double, wavelength_bases>;

    NLOptInfo solver = {
      .n            = wavelength_bases,
      .algo         = NLOptAlgo::LD_SLSQP,
      .form         = NLOptForm::eMinimize,
      .x_init       = Basis::BVec(0.5).cast<double>().eval(),
      .rel_func_tol = 1e-4,
      .rel_xpar_tol = 1e-2,
    };

    // Construct basis matrix
    auto B = info.basis.func.cast<double>().eval();

    // Construct basis matrix and boundary vectors
    auto basis = info.basis.func.cast<double>().eval();
    
    // Objective function minimizes l2 norm over spectral distribution
    solver.objective = detail::func_norm(info.basis.func, Spec::Zero());

    // Add color system equality constraints, upholding spectral surface metamerism
    {
      // auto A = (info.base_system.transpose() * info.basis.func.matrix()).eval();
      // auto o = (info.base_system.transpose() * info.basis.mean.matrix()).eval();
      // auto b = (info.base_signal - o.array()).eval();
      auto A = (info.base_system.transpose() * info.basis.func.matrix()).eval();
      auto b = info.base_signal;
      solver.eq_constraints.push_back(detail::func_norm(A, b));
    } // for (uint i)

    // Add boundary inequality constraints, upholding spectral 0 <= x <= 1
    for (uint i = 0; i < wavelength_samples; ++i) {
      auto  a  = info.basis.func.row(i).eval();
      float ub = 1.f; /* 1.f - info.basis.mean[i]; */
      float lb = ub - 1.f;
      solver.nq_constraints.push_back(detail::func_dot( a,  ub));
      solver.nq_constraints.push_back(detail::func_dot(-a, -lb));
    } // for (uint i)
    
    // Double, transpose versions of interreflection system
    std::vector<DCMFS> C(info.refl_systems.size());
    rng::transform(info.refl_systems, C.begin(), 
      [&](const CMFS &cmfs) { return cmfs.transpose().cast<double>().eval(); });
      
    Colr interrefl_err = 0.f;

    // Add interreflection result constraint
    {
      // Colr o = 0.f;
      // for (uint i = 0; i < info.refl_systems.size(); ++i) {
      //   float p = static_cast<float>(i);
      //   Spec mean = info.basis.mean; //.pow(p);
      //   fmt::print("mean = {}\n", mean);
      //   o += (info.refl_systems[i].transpose() * mean.matrix()).transpose().array().eval();
      // }

      // for (const auto &cs : info.refl_systems)
      //   o += (cs.transpose() * info.basis.mean.matrix()).transpose().array().eval();
      // auto b = (info.refl_signal - o).cast<double>().eval();

      auto b = info.refl_signal.cast<double>().eval();
      // DSpec dmean = info.basis.mean.cast<double>().eval();

      /* solver.eq_constraints.push_back(
        [&C, &B, &interrefl_err, b] (eig::Map<const eig::VectorXd> x, eig::Map<eig::VectorXd> g) {
          DSpec R = (B * x).array(); // + info.basis.mean.cast<double>().eval();

          eig::Array3d colr = 0.0;
          for (uint i = 1; i < C.size(); ++i) {
            double p = static_cast<double>(i);
            DSpec pr = R.array().pow(p);
            colr += (C[i].transpose() * pr).array();
          }
          
          if (g.size()) {
            BSpec grad = 0.0;
            for (uint i = 1; i < C.size(); ++i) {
              double p = static_cast<double>(i);
              DSpec dr = R.array().pow(p - 1.0);

              auto vv = (C[i].transpose().array().colwise() * dr).eval();
              // grad += p * (C[i].transpose().rowwise() )
            }
            
          }

          return 0.0;
      }); */

      for (uint j = 0; j < 3; ++j) {
        solver.eq_constraints.push_back(
          [&C, &B, &interrefl_err, /* &dmean, */ j, b] (eig::Map<const eig::VectorXd> x, eig::Map<eig::VectorXd> g) {
            DSpec R = (B * x).array(); // + info.basis.mean.cast<double>().eval();

            double err = 0.0; // C[0].row(j).transpose().sum();
            for (uint i = 1; i < C.size(); ++i) {
              double p = static_cast<double>(i);
              DSpec pr = R.array().pow(p);
              err = err + C[i].row(j).transpose().dot(pr);
                        // - (b[j] - C[i].row(j).transpose().dot(dmean.array().pow(p).matrix()));
            }

            if (g.size()) {
              BSpec grad = 0.0;
              for (uint i = 1; i < C.size(); ++i) {
                double p = static_cast<double>(i);
                DSpec dr = R.array().pow(p - 1.0);
                grad += p * (C[i].row(j).transpose().cwiseProduct(dr).transpose() * B);
              }
              g = grad;
            }
            
            interrefl_err[j] = err - b[j];
            return err - b[j];
        });
      }
    }

    // Run solver and return recovered spectral distribution
    NLOptResult r = solve(solver);
    return (info.basis.func * r.x.cast<float>()).array() + info.basis.mean;
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
      .rel_func_tol = 1e-4,
      .rel_xpar_tol = 1e-2,
    };

    // Add color system equality constraints, upholding spectral metamerism
    for (uint i = 0; i < info.systems_i.size(); ++i) {
      auto A = (info.systems_i[i].transpose() * info.basis.func.matrix()).eval();
      auto o = (info.systems_i[i].transpose() * info.basis.mean.matrix()).eval();
      auto b = (info.signals_i[i] - o.array()).eval();
      solver.eq_constraints.push_back(detail::func_norm(A, b));
    } // for (uint i)

    // Add boundary inequality constraints, upholding spectral 0 <= x <= 1
    for (uint i = 0; i < wavelength_samples; ++i) {
      auto  a  = info.basis.func.row(i).eval();
      float ub = 1.f - info.basis.mean[i];
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
        // Define objective function components: 64x1
        using DSpec = eig::Array<double, wavelength_samples, 1>;
        std::vector<DSpec> C(info.systems_j.size());
        rng::transform(info.systems_j, C.begin(), 
          [&](const CMFS &cmfs) { return (cmfs * info.samples[i].matrix()).array().cast<double>().eval(); });
        
        local_solver.objective = 
          [&C, B = info.basis.func.cast<double>().eval()]
          (eig::Map<const eig::VectorXd> x, eig::Map<eig::VectorXd> g) -> double {
            auto   X   = (B * x).eval();
            double obj = 0.0;

            for (uint j = 1; j <  C.size(); ++j) {
              double p  = static_cast<double>(j);
              
              obj += C[j].matrix().dot(X.array().pow(p).matrix());
              if (g.data())
                g += B.transpose() 
                   * (p * (C[j] * X.array().pow(p - 1.0))).matrix();              
            }

            return obj;
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