#include <metameric/core/nlopt.hpp>
#include <metameric/core/utility.hpp>
#include <exception>

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

  std::vector<Spec> nl_generate_mmv_boundary_spec(const GenerateMMVBoundaryInfo &info) {
    met_trace();

    // NLOptInfo solver = {
    //   .n      = wavelength_bases,
    //   .algo   = NLOptAlgo::LD_SLSQP,
    //   .form   = NLOptForm::eMaximize,
    //   .x_init = Basis::BVec(0.5).cast<double>().eval(),
    //   .max_iters    = 10,
    //   .rel_func_tol = 1e-3,
    //   .rel_xpar_tol = 1e-2,
    // };
    
    // // Generate color system spectra which use basis functions
    // auto csys_j = (info.system_j.transpose() * info.basis.func).eval();
    // auto csys_i = std::vector<Syst>(info.systems_i.size());
    // rng::transform(info.systems_i, csys_i.begin(),
    //   [&](const auto &m) { return (m.transpose() * info.basis.func).eval(); });    


    return { };
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

    // Use c-api to avoid conversion to std::vector when data is already owned by eigen object
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
