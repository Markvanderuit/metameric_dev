#include <metameric/core/nlopt.hpp>
#include <metameric/core/ranges.hpp>
#include <algorithm>
#include <exception>

namespace met {
  namespace detail {
    // Function wrapper to encapsulate pointers in mapped eigen vectors, then to pass
    // these to a constraint or objective function capture
    constexpr auto func_wrapper = [](uint n, const double *x, double *g, void *data) -> double {
      // Dereference passed pointer to capture, and forward data in mapped Eigen objects
      // Only pass on gradient if present, not all algorithms require or provide gradient
      auto x_ = eig::Map<const eig::VectorXd>(x, n);
      auto g_ = g 
              ? eig::Map<eig::VectorXd>(g, n) 
              : eig::Map<eig::VectorXd>(nullptr, 0);
      return static_cast<NLOptInfo::Capture *>(data)->operator()(x_, g_);
    };

    // Function wrapper to encapsulate pointers in mapped eigen vectors, then to pass
    // these to a vector constraint function capture
    constexpr auto func_wrapper_v = [](uint m, double *r, uint n, const double *x, double *g, void *data) -> void {
      // Dereference passed pointer to capture, and forward data in mapped Eigen objects
      // Only pass on gradient if present, not all algorithms require or provide gradient
      auto r_ = eig::Map<eig::VectorXd>(r, m);
      auto x_ = eig::Map<const eig::VectorXd>(x, n);
      auto g_ = g 
              ? eig::Map<eig::MatrixXd>(g, n, m) 
              : eig::Map<eig::MatrixXd>(nullptr, 0, 0);
      return static_cast<NLOptInfo::VectorCapture *>(data)->operator()(r_, x_, g_);
    };
  } // namespace detail

  NLOptResult solve(NLOptInfo &info) {
    met_trace();

    // Optimization object holder
    nlopt::opt opt(info.algo, info.n);

    // Specify objective function
    if (info.form == NLOptForm::eMinimize) {
      opt.set_min_objective(detail::func_wrapper, &info.objective);
    } else {
      opt.set_max_objective(detail::func_wrapper, &info.objective);
    }

    // Add equality/inequality constraints
    for (auto &cstr : info.eq_constraints)
      opt.add_equality_constraint(detail::func_wrapper, &cstr.f, cstr.tol);
    for (auto &cstr : info.nq_constraints)
      opt.add_inequality_constraint(detail::func_wrapper, &cstr.f, cstr.tol);
    for (auto &cstr : info.eq_constraints_v)
      opt.add_equality_mconstraint(detail::func_wrapper_v, &cstr.f, std::vector<double>(cstr.n, cstr.tol));
    for (auto &cstr : info.nq_constraints_v)
      opt.add_inequality_mconstraint(detail::func_wrapper_v, &cstr.f, std::vector<double>(cstr.n, cstr.tol));

    // Specify optional upper/lower bounds
    std::vector<double> upper(range_iter(info.upper));
    std::vector<double> lower(range_iter(info.lower));
    if (!upper.empty())
      opt.set_upper_bounds(upper);
    if (!lower.empty())
      opt.set_lower_bounds(lower);

    // Specify optional stopping criteria and tolerances
    if (info.rel_xpar_tol) opt.set_xtol_rel(*info.rel_xpar_tol);
    if (info.max_time)     opt.set_maxtime(*info.max_time);
    if (info.max_iters)    opt.set_maxeval(*info.max_iters);
    if (info.stopval)      opt.set_stopval(*info.stopval);
    
    // Placeholder for 'x' because the library enforces std::vector :S
    std::vector<double> x(info.n, 0.0);
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

    // Copy over solution to return value
    result.x.resize(info.n);
    rng::copy(x, result.x.begin());
    return result;
  }
} // namespace met
