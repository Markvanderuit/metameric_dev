#include <metameric/core/nlopt.hpp>
#include <algorithm>
#include <exception>
#include <execution>

namespace met {
  Spec nl_generate_spectrum(GenerateSpectrumInfo info) {
    met_trace();

    NLOptInfo solver = {
      .n      = wavelength_bases,
      .algo   = NLOptAlgo::LD_SLSQP,
      .form   = NLOptForm::eMinimize,
      .x_init = Basis::BVec(0.5).cast<double>().eval(),
      .max_iters    = 10,
      .rel_func_tol = 1e-3,
      .rel_xpar_tol = 1e-2,
    };

    // Objective function minimizes squared l2 norm
    uint n_objc_calls = 0;
    solver.objective = [&](eig::Map<const eig::VectorXd> x, eig::Map<eig::VectorXd> grad) {
      n_objc_calls++;
      if (grad.size())
        grad = (2.0 * x).eval();
      return x.dot(x);
    };
    
    // Construct basis matrix and boundary vectors
    auto basis = info.basis.func.cast<double>().eval();
    Spec upper_bounds = Spec(1.0) - info.basis.mean;
    Spec lower_bounds = upper_bounds - Spec(1.0); 

    // Add boundary inequality constraints
    for (uint i = 0; i < wavelength_samples; ++i) {
      solver.nq_constraints.push_back(
        [A = basis.row(i).eval(), b = upper_bounds[i]]
        (eig::Map<const eig::VectorXd> x, eig::Map<eig::VectorXd> grad) {
          double f = A.dot(x);
          if (grad.size())
            grad = A;
          return f - b;
      });
      solver.nq_constraints.push_back(
        [A = basis.row(i).eval(), b = lower_bounds[i]]
        (eig::Map<const eig::VectorXd> x, eig::Map<eig::VectorXd> grad) {
          double f = -A.dot(x);
          if (grad.size())
            grad = -A;
          return f + b;
      });
    }

    // Add color system equality constraints
    for (uint i = 0; i < info.systems.size(); ++i) {
      Colr o = (info.systems[i].transpose() * info.basis.mean.matrix()).transpose().eval();
      auto A = (info.systems[i].transpose().cast<double>() * basis).eval();
      auto b = (info.signals[i] - o).cast<double>().eval();

      for (uint j = 0; j < 3; ++j) {
        solver.eq_constraints.push_back(
          [A = A.row(j).eval(), b = b[j]]
          (eig::Map<const eig::VectorXd> x, eig::Map<eig::VectorXd> grad) {
            double f = A.dot(x);
            if (grad.size())
              grad = A;
            return f - b;
        });
      } // for (uint j)
    } // for (uint i)

    // Optimize result and return
    NLOptResult r = solve(solver);
    Spec s = info.basis.mean + Spec((basis * r.x).cast<float>());
    return s;
  }

  std::vector<Spec> nl_generate_ocs_boundary_spec(const GenerateOCSBoundaryInfo &info) {
    met_trace();

    // Construct basis matrix
    auto B = info.basis.func.cast<double>().eval();
    auto basis_trf = 
      [B](const auto &csys) { return (csys.transpose().cast<double>() * B).eval(); };
      
    NLOptInfo solver = {
      .n            = wavelength_bases,
      .algo         = NLOptAlgo::LD_SLSQP,
      .form         = NLOptForm::eMaximize,
      .x_init       = eig::Vector<double, wavelength_bases>(0.5),
      // .max_iters    = 100,
      .rel_func_tol = 1e-2,
      .rel_xpar_tol = 1e-3,
    };

    // Construct orthogonal matrix used during maximiation
    auto S = info.system.cast<double>().eval();
    eig::JacobiSVD<decltype(S)> svd;
    svd.compute(S, eig::ComputeFullV);
    auto U = (S * svd.matrixV() * svd.singularValues().asDiagonal().inverse()).eval();

    // Add [0, 1] boundary inequality constraints
    for (uint i = 0; i < wavelength_samples; ++i) {
      solver.nq_constraints.push_back(
        [B = B.row(i).eval()]
        (eig::Map<const eig::VectorXd> x, eig::Map<eig::VectorXd> g) {
          if (g.size())
            g = B;
          return B.dot(x) - 1.0;
      });
      solver.nq_constraints.push_back(
        [B = B.row(i).eval()]
        (eig::Map<const eig::VectorXd> x, eig::Map<eig::VectorXd> g) {
          if (g.size())
            g = -B;
          return (-B).dot(x);
      });
    } // for (uint i)

    // Output data vector
    std::vector<Spec> output(info.samples.size());

    // Parallel solve for basis function weights defining OCS boundary spectra
    #pragma omp parallel
    {
      // Per thread copy of current solver parameter set
      NLOptInfo local_solver = solver;

      #pragma omp for
      for (int i = 0; i < info.samples.size(); ++i) {
        // Define objective function: max (Uk)^T (Bx)^p -> max C^T (Bx)^p
        auto C = (U * info.samples[i].matrix().cast<double>()).eval();

        local_solver.objective = 
          [&C, &B]
          (eig::Map<const eig::VectorXd> x, eig::Map<eig::VectorXd> g) -> double {
            using Spec = eig::Vector<double, wavelength_samples>;

            // Linear function:
            // - f(x) = C^T Bx
            // - d(f) = C^T B
            Spec px = B * x;
            if (g.size())
              g = (C.transpose() * B).transpose().eval();
            return C.dot(px);
        };

        // Obtain result from solver
        NLOptResult r = solve(local_solver);
        // fmt::print("{} -> {} steps, {} value, {} code\n", i, objective_steps, r.objective, r.code);

        // Return spectral result, raised to requested power
        Spec s = (B * r.x).cast<float>();
        output[i] = s.cwiseMin(1.f).cwiseMax(0.f);
      } // for (int i)
    } // parallel

    // Filter NaNs at underconstrained output and strip redundancy from return 
    std::erase_if(output, [](Spec &c) { return c.isNaN().any(); });
    std::unordered_set<
      Spec, 
      decltype(Eigen::detail::matrix_hash<float>), 
      decltype(Eigen::detail::matrix_equal)
    > output_unique(range_iter(output));
    return std::vector<Spec>(range_iter(output_unique));
  }

  std::vector<Colr> nl_generate_ocs_boundary_colr(const GenerateOCSBoundaryInfo &info) {
    met_trace();

    auto spectra = nl_generate_ocs_boundary_spec(info);
    std::vector<Colr> colors(spectra.size());
    std::transform(std::execution::par_unseq, range_iter(spectra), colors.begin(),
    [&](const auto &s) -> Colr {
      return (info.system.transpose() * s.matrix()).eval();
    });
    return colors;
  }

  std::vector<Spec> nl_generate_mmv_boundary_spec(const NLGenerateMMVBoundaryInfo &info, double power, bool switch_power) {
    met_trace();

    // Construct basis matrix
    auto B = info.basis.func.cast<double>().eval();
    auto basis_trf = 
      [B](const auto &csys) { return (csys.transpose().cast<double>() * B).eval(); };

    NLOptInfo solver = {
      .n            = wavelength_bases,
      .algo         = NLOptAlgo::LD_SLSQP,
      .form         = NLOptForm::eMaximize,
      .x_init       = eig::Vector<double, wavelength_bases>(0.5),
      // .max_iters    = 100,
      .rel_func_tol = 1e-2,
      .rel_xpar_tol = 1e-3,
    };

    // Construct orthogonal matrix used during maximiation
    auto S = rng::fold_left_first(info.systems_j, std::plus<CMFS> {}).value().cast<double>().eval();
    eig::JacobiSVD<decltype(S)> svd;
    svd.compute(S, eig::ComputeFullV);
    auto U = (S * svd.matrixV() * svd.singularValues().asDiagonal().inverse()).eval();

    // Add color system equality constraints
    for (uint i = 0; i < info.systems_i.size(); ++i) {
      auto A = basis_trf(info.systems_i[i]);
      auto b = info.signals_i[i].cast<double>().eval();
      for (uint j = 0; j < 3; ++j) {
        solver.eq_constraints.push_back(
          [A = A.row(j).eval(), b = b[j]]
          (eig::Map<const eig::VectorXd> x, eig::Map<eig::VectorXd> g) {
            using Spec = eig::Vector<double, wavelength_samples>;

            // Linear function:
            // - f(x) = A^T Bx - b == 0
            // - d(f) = A^T B
            if (g.size())
              g = A;
            return A.dot(x) - b;
        });
      } // for (uint j)
    } // for (uint i)

    // Add [0, 1] boundary inequality constraints
    for (uint i = 0; i < wavelength_samples; ++i) {
      solver.nq_constraints.push_back(
        [B = B.row(i).eval()]
        (eig::Map<const eig::VectorXd> x, eig::Map<eig::VectorXd> g) {
          if (g.size())
            g = B;
          return B.dot(x) - 1.0;
      });
      solver.nq_constraints.push_back(
        [B = B.row(i).eval()]
        (eig::Map<const eig::VectorXd> x, eig::Map<eig::VectorXd> g) {
          if (g.size())
            g = -B;
          return (-B).dot(x);
      });
    }

    // Output data vector
    std::vector<Spec> output(info.samples.size());

    // Parallel solve for basis function weights defining OCS boundary spectra
    #pragma omp parallel
    {
      // Per thread copy of current solver parameter set
      NLOptInfo local_solver = solver;

      #pragma omp for
      for (int i = 0; i < info.samples.size(); ++i) {
        // Define objective function: max (Uk)^T (Bx)^p -> max C^T (Bx)^p
        auto C = (U * info.samples[i].matrix().cast<double>()).eval();

        local_solver.objective = 
          [&C, &B, p = power, run_full_power = switch_power]
          (eig::Map<const eig::VectorXd> x, eig::Map<eig::VectorXd> g) -> double {
            using Spec = eig::Vector<double, wavelength_samples>;
            
            // Power function:
            // - f(x) = C^T (Bx)^p
            // - d(f) = p * (C x (Bx)^{p - 1})^T * B
            Spec X = B * x;
            Spec px, dx;
            if (run_full_power) {
              px = X.array().pow(p).eval();
              dx = X.array().pow(p - 1.0).eval();
            } else {
              // px = X.array().pow(/* std::min(p, 4.0) */8.0).eval();
              // dx = X.array().pow(/* std::min(p, 4.0) */8.0 - 1.0).eval();
              px = p > 1.f ? X.cwiseProduct(X) : X;
              dx = p > 1.f ? X : 1.f;
            }
            if (g.size())
              g = p * (C.cwiseProduct(dx).transpose() * B).array();
            return C.dot(px);

            // Linear function:
            // - f(x) = C^T Bx
            // - d(f) = C^T B
            /* Spec px = B * x;
            if (g.size())
              g = (C.transpose() * B).transpose().eval();
            return C.dot(px); */
        };

        // Obtain result from solver
        NLOptResult r = solve(local_solver);

        // Return spectral result, raised to requested power
        Spec s = (B * r.x).cast<float>();
        
        output[i] = s.pow(power).cwiseMin(1.f).cwiseMax(0.f);
      } // for (int i)
    }

    // Filter NaNs at underconstrained output and strip redundancy from return 
    std::erase_if(output, [](Spec &c) { return c.isNaN().any(); });
    /* std::unordered_set<
      Spec, 
      decltype(Eigen::detail::matrix_hash<float>), 
      decltype(Eigen::detail::matrix_equal)
    > output_unique(range_iter(output)); */
    return output; // std::vector<Spec>(range_iter(output_unique));
  }

  NLMMVBoundarySet nl_generate_mmv_boundary_colr(const NLGenerateMMVBoundaryInfo &info, double n_scatters, bool switch_power) {
    met_trace();

    // Generate unique boundary spectra
    auto spectra = nl_generate_mmv_boundary_spec(info, n_scatters, switch_power);

    // Transform to non-unique colors
    std::vector<Colr> colors(spectra.size());
    std::transform(std::execution::par_unseq, range_iter(spectra), colors.begin(),
      [&](const auto &s) -> Colr {  return (info.system_j.transpose() * s.matrix()).eval(); });

    // Collapse return value to unique colors
    return NLMMVBoundarySet(range_iter(colors));
  }

  NLOptResult solve(NLOptInfo &info) {
    met_trace();

    constexpr auto func_wrapper = [](uint n, const double *x, double *g, void *data) -> double {
      // Dereference passed pointer to capture, and forward data in mapped Eigen objects
      // Only pass gradient on if it is present, not all algorithms provide gradient
      auto x_ = eig::Map<const eig::VectorXd>(x, n);
      auto g_ = g ? eig::Map<eig::VectorXd>(g, n) : eig::Map<eig::VectorXd>(nullptr, 0);
      return static_cast<NLOptInfo::Capture *>(data)->operator()(x_, g_);
    };

    nlopt::opt opt(info.algo, info.n);

    if (info.form == NLOptForm::eMinimize) {
      opt.set_min_objective(func_wrapper, &info.objective);
    } else {
      opt.set_max_objective(func_wrapper, &info.objective);
    }

    for (auto &eq : info.eq_constraints)
      opt.add_equality_constraint(func_wrapper, &eq, info.rel_func_tol.value_or(0.0));
    for (auto &nq : info.nq_constraints)
      opt.add_inequality_constraint(func_wrapper, &nq, info.rel_func_tol.value_or(0.0));

    // Specify optional upper/lower bounds
    std::vector<double> upper(range_iter(info.upper));
    std::vector<double> lower(range_iter(info.lower));
    if (!upper.empty())
      opt.set_upper_bounds(upper);
    if (!lower.empty())
      opt.set_lower_bounds(lower);

    // Optional stopping criteria and tolerances
    if (info.rel_xpar_tol) opt.set_xtol_rel(*info.rel_xpar_tol);
    if (info.max_time)     opt.set_maxtime(*info.max_time);
    if (info.max_iters)    opt.set_maxeval(*info.max_iters);
    if (info.stopval)      opt.set_stopval(*info.stopval);
    
    // Placeholder for 'x' because the library enforces std::vector :S
    std::vector<double> x(info.n);
    if (info.x_init.size()) 
      rng::copy(info.x_init, x.begin());

    // Run optimization and store results
    NLOptResult result;
    try {
      result.code = opt.optimize(x, result.objective);
    } catch (const nlopt::roundoff_limited &e) {
      // ... fails silently for now
    } catch (const nlopt::forced_stop &e) {
      // ... fails silently for now
    } catch (const std::exception &e) {
      fmt::print("{}\n", e.what());
    }

    // Copy over solution to result object
    result.x.resize(info.n);
    rng::copy(x, result.x.begin());

    return result;
  }
} // namespace met
