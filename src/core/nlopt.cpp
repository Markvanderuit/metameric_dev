#include <metameric/core/nlopt.hpp>
#include <metameric/core/utility.hpp>
#include <exception>

namespace met {
  Spec nl_generate_spectrum(GenerateSpectrumInfo info) {
    met_trace();

    NLOptInfo solver = {
      .n      = wavelength_samples,
      .algo   = NLOptAlgo::LD_SLSQP,
      .form   = NLOptForm::eMinimize,
      .x_init = Spec(0.5).cast<double>().eval(),
      .upper  = Spec(1).cast<double>().eval(),
      .lower  = Spec(0).cast<double>().eval(),
      .max_iters = 11,
      .relative_func_tol = 0.0,
      .relative_xpar_tol = 0.0,
    };

    uint n_objc_calls = 0;
    solver.objective = [&](eig::Map<const eig::VectorXd> x, eig::Map<eig::VectorXd> grad) -> double {
      n_objc_calls++;
      if (grad.size())
        grad = (2.0 * x).eval(); // eig::Vector<double, wavelength_samples>(1);
      return x.dot(x);
    };

    auto A = info.systems[0].transpose().cast<double>().eval();
    auto b = info.signals[0].cast<double>().eval();
    for (uint i = 0; i < 3; ++i) {
      solver.eq_constraints.push_back([&](eig::Map<const eig::VectorXd> x, eig::Map<eig::VectorXd> grad) -> double {
        auto a = A.row(i).eval();
        if (grad.size())
          grad = a;
        auto f = (a * x).eval();
        return f[0] - b[i];
      });
    }

    NLOptResult r = solve(solver);
    fmt::print("Halted after {} iters\n", n_objc_calls);
    // if (r.code)
    //   fmt::print("Error: {}\n", r.code);
    // fmt::print("Found f{} = {} tolerance after {} iterations\n", r.x, r.objective, n_objc_calls);
    return Spec(r.x.cast<float>());
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
      opt.add_equality_constraint(func_wrapper, &eq, info.relative_func_tol);
    for (auto &nq : info.nq_constraints)
      opt.add_inequality_constraint(func_wrapper, &nq, info.relative_func_tol);

    // Use c-api to avoid conversion to std::vector when data is already owned by eigen object
    std::vector<double> upper(range_iter(info.upper));
    std::vector<double> lower(range_iter(info.lower));
    if (!upper.empty()) opt.set_upper_bounds(upper);
    if (!lower.empty()) opt.set_lower_bounds(lower);

    // Stopping criteria
    // opt.set_xtol_rel(info.relative_xpar_tol);
    // opt.set_maxtime()
    // if (info.max_iters) opt.set_maxeval(*info.max_iters);
    // if (info.stopval) opt.set_stopval(*info.stopval);
    // opt.set_maxtime(0.01);
    opt.set_maxeval(11);
    
    // Placeholder for 'x' because the library enforces std::vector :S
    std::vector<double> x(info.n);
    if (info.x_init.size()) 
      rng::copy(info.x_init, x.begin());

    // Run optimization and store results
    NLOptResult result;
    try {
      result.code = opt.optimize(x, result.objective);
    } catch (const std::exception &e) {
      // fmt::print("{}\n", e.what());
    }

    // Copy over solution to result object
    result.x.resize(info.n);
    rng::copy(x, result.x.begin());

    return result;
  }
} // namespace met
