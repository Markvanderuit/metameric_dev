#include <metameric/core/nlopt.hpp>
#include <metameric/core/utility.hpp>
#include <algorithm>
#include <exception>
#include <execution>
#include <vector>
#include <unordered_set>

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

  std::vector<Spec> nl_generate_mmv_boundary_spec(const GenerateMMVBoundaryInfo &info, double power) {
    met_trace();

    // Construct basis matrix
    constexpr uint N = wavelength_bases;
    auto basis = info.basis.func.cast<double>().eval();
    // auto basis = eig::Matrix<double, wavelength_samples, wavelength_samples>::Identity().eval();
    auto basis_trf = 
      [b = basis](const auto &csys) { return (csys.transpose().cast<double>() * b).eval(); };

    NLOptInfo solver = {
      .n            = N,
      .algo         = NLOptAlgo::LD_SLSQP,
      .form         = NLOptForm::eMaximize,
      .x_init       = eig::Vector<double, N>(0.5),
      .max_iters    = 30,
      .rel_func_tol = 1e-5,
      .rel_xpar_tol = 1e-3,
    };

    // Construct color system spectra over basis matrix
    auto csys_j = basis_trf(info.system_j);
    auto csys_i = vws::transform(info.systems_i, basis_trf) | rng::to<std::vector>();
    
    // Construct orthogonal matrix used during maximiation
    // auto S = csys_j.transpose().eval();
    // eig::JacobiSVD<decltype(S)> svd;
    // svd.compute(S, eig::ComputeFullV);
    // auto U = (S * svd.matrixV() * svd.singularValues().asDiagonal().inverse()).eval();
    auto S = info.system_j.cast<double>().eval();
    eig::JacobiSVD<decltype(S)> svd;
    svd.compute(S, eig::ComputeFullV);
    auto U = (S * svd.matrixV() * svd.singularValues().asDiagonal().inverse()).eval();

    // Add color system equality constraints
    for (uint i = 0; i < csys_i.size(); ++i) {
      auto A = csys_i[i];
      auto b = info.signals_i[i].cast<double>().eval();
      for (uint j = 0; j < 3; ++j) {
        solver.eq_constraints.push_back(
          [A = A.row(j).transpose().eval(), b = b[j], p = power]
          (eig::Map<const eig::VectorXd> x, eig::Map<eig::VectorXd> grad) {
            if (grad.size())
              grad = A;
              // grad = (p * A.array() * x.array().pow(p - 1.0));
            return A.dot(x) - b;
            // return A.dot(x.array().pow(p).matrix()) - b;
        });
      } // for (uint j)
    } // for (uint i)

    // Add [0, 1] boundary inequality constraints
    for (uint i = 0; i < wavelength_samples; ++i) {
      solver.nq_constraints.push_back(
        [A = basis.row(i).transpose().eval()]
        (eig::Map<const eig::VectorXd> x, eig::Map<eig::VectorXd> grad) {
          if (grad.size())
            grad = A;
          return A.dot(x) - 1.0;
      });
      solver.nq_constraints.push_back(
        [A = basis.row(i).transpose().eval()]
        (eig::Map<const eig::VectorXd> x, eig::Map<eig::VectorXd> grad) {
          if (grad.size())
            grad = -A;
          return (-A).dot(x);
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
        // Define objective function: max_x Cx
        auto C = (U * info.samples[i].matrix().cast<double>()).eval();
        uint objective_steps = 0;
        local_solver.objective = 
          [&objective_steps, C, &basis, power](eig::Map<const eig::VectorXd> x, eig::Map<eig::VectorXd> grad) -> double {
            using Spec = eig::Vector<double, wavelength_samples>;

            objective_steps++;
            
            // f(x)      = C^T x
            // ddx(f(x)) = C
            /* if (grad.size())
              grad = C;
            return (C.transpose() * x).coeff(0); */

            // f(x)    = C^T (Bx)^p
            // d(f(x)) = p * (C x (Bx)^{p - 1})^T * B
            Spec px = (basis * x).array().pow(power);
            Spec dx = (basis * x).array().pow(power - 1.0);
            if (grad.size())
              grad = power * (C.cwiseProduct(dx).transpose() * basis).array();
            return C.dot(px);
            // return (C.transpose() * px).coeff(0);

            // f(x)    = C^T Bx
            // d(f(x)) = C^T B
            // if (grad.size())
            //   grad = (C.transpose() * basis).transpose().eval();
            // return (C.transpose() * (basis * x)).coeff(0);

            // f(x) = C^T x^p
            // ddx  = p(C * x^(p - 1))
            // if (grad.size())
            //   grad = C.array() * (power * x.array().pow(power - 1.0)).eval();
            // return (C.transpose() * x.array().pow(power).matrix()).coeff(0);

            // return 0.0;
        };

        // Obtain result from solver
        NLOptResult r = solve(local_solver);
        output[i] = Spec((basis * r.x).array().pow(power).cwiseMin(1.f).cwiseMax(0.f).cast<float>());
        // fmt::print("{} -> {} steps, {} value, {} code\n", i, objective_steps, r.objective, r.code);
      } // for (int i)
    }

    // Filter NaNs at underconstrained output and strip redundancy from return 
    std::erase_if(output, [](Spec &c) { return c.isNaN().any(); });
    std::unordered_set<
      Spec, 
      decltype(Eigen::detail::matrix_hash<float>), 
      decltype(Eigen::detail::matrix_equal)
    > output_unique(range_iter(output));
    return std::vector<Spec>(range_iter(output_unique));
  }

  std::vector<Colr> nl_generate_mmv_boundary_colr(const GenerateMMVBoundaryInfo &info) {
    met_trace();
    auto spectra = nl_generate_mmv_boundary_spec(info, 1);
    std::vector<Colr> colors(spectra.size());
    std::transform(std::execution::par_unseq, range_iter(spectra), colors.begin(),
    [&](const auto &s) -> Colr {
      return (info.system_j.transpose() * s.matrix()).eval();
    });
    return colors;
  }

  void test_nlopt() {
    met_trace();

    NLOptInfo info = {
      .n      = 2,
      .algo   = NLOptAlgo::LD_MMA,
      .form   = NLOptForm::eMinimize,
      .x_init = eig::Vector2d { 1.234, 5.678 },
      .lower  = eig::Vector2d { -HUGE_VAL, 0 },
    };

    // Add objective function and inequality constraints
    uint n_objc_calls = 0;
    info.objective = [&](eig::Map<const eig::VectorXd> x, eig::Map<eig::VectorXd> grad) -> double {
      n_objc_calls++;
      if (grad.size())
        grad = eig::Vector2d { 0.0, 0.5 / std::sqrt(x[1]) };
      return std::sqrt(x[1]);
    };
    info.nq_constraints.push_back([](eig::Map<const eig::VectorXd> x, eig::Map<eig::VectorXd> grad) -> double {
      auto d_mul = 2.0 * x[0];
      if (grad.size())
        grad = eig::Vector2d { 3.0 * 2.0 * (d_mul) * (d_mul), -1.0 };
      return d_mul * d_mul * d_mul - x[1];
    });
    info.nq_constraints.push_back([](eig::Map<const eig::VectorXd> x, eig::Map<eig::VectorXd> grad) -> double {
      auto d_mul = -1.0 * x[0] + 1.0;
      if (grad.size())
        grad = eig::Vector2d { 3.0 * -1.0 * (d_mul) * (d_mul), -1.0 };
      return d_mul * d_mul * d_mul - x[1];
    });

    NLOptResult r = solve(info);
    if (r.code)
      fmt::print("Error: {}\n", r.code);
    fmt::print("Found f{} = {} tolerance after {} iterations\n", r.x, r.objective, n_objc_calls);
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
    /* else
      opt.set_upper_bounds(HUGE_VAL); */
    if (!lower.empty())
      opt.set_lower_bounds(lower);
    /* else
      opt.set_lower_bounds(-HUGE_VAL); */

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
    } catch (const std::exception &e) {
      // Blerp
      fmt::print("{}\n", e.what());
    }

    // Copy over solution to result object
    result.x.resize(info.n);
    rng::copy(x, result.x.begin());

    return result;
  }
} // namespace met
