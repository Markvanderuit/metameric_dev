#include <metameric/core/solver.hpp>
#include <metameric/core/ranges.hpp>
#include <metameric/core/spectrum.hpp>
#include <exception>

namespace nlopt {
  using namespace met;

  template <uint N>
  Result<N> solve(Wrapper<N> &info) {
    met_trace();

    // Commonly used types from info object
    using vec = Wrapper<N>::vec;
    using mat = Wrapper<N>::mat;
    
    // Define function wrapper to encapsulate pointers in mapped eigen vectors, then to pass
    // these to a constraint or objective function capture
    constexpr auto func_wrapper = [](uint n, const double *x, double *g, void *data) -> double {
      // Dereference passed pointer to capture, and forward data in mapped Eigen objects
      // Only pass on gradient if present, not all algorithms require or provide gradient
      auto x_ = eig::Map<const vec>(x);
      auto g_ = g 
              ? eig::Map<vec>(g) 
              : eig::Map<vec>(nullptr, 0);
      return static_cast<Wrapper<N>::Constraint::Capture *>(data)->operator()(x_, g_);
    };

    // Define function wrapper to encapsulate pointers in mapped eigen vectors, then to pass
    // these to a vector constraint function capture
    constexpr auto func_wrapper_v = [](uint m, double *r, uint n, const double *x, double *g, void *data) -> void {
      // Dereference passed pointer to capture, and forward data in mapped Eigen objects
      // Only pass on gradient if present, not all algorithms require or provide gradient
      auto r_ = eig::Map<eig::VectorXd>(r, m);
      auto x_ = eig::Map<const vec>(x);
      auto g_ = g 
              ? eig::Map<mat>(g, N, m) 
              : eig::Map<mat>(nullptr, 0, 0);
      return static_cast<Wrapper<N>::ConstraintV::Capture *>(data)->operator()(r_, x_, g_);
    };

    // Optimization object descriptor
    opt desc(info.algo, N);

    // Specify objective function
    if (info.dirc == direction::eMinimize) {
      desc.set_min_objective(func_wrapper, &info.objective);
    } else {
      desc.set_max_objective(func_wrapper, &info.objective);
    }
    
    // Add equality/inequality constraints
    for (auto &cstr : info.eq_constraints)
      desc.add_equality_constraint(func_wrapper, &cstr.f, cstr.tol);
    for (auto &cstr : info.nq_constraints)
      desc.add_inequality_constraint(func_wrapper, &cstr.f, cstr.tol);
    for (auto &cstr : info.eq_constraints_v)
      desc.add_equality_mconstraint(func_wrapper_v, &cstr.f, std::vector<double>(cstr.n, cstr.tol));
    for (auto &cstr : info.nq_constraints_v)
      desc.add_inequality_mconstraint(func_wrapper_v, &cstr.f, std::vector<double>(cstr.n, cstr.tol));

    // Specify optional upper/lower bounds
    if (auto v = info.upper)
      desc.set_upper_bounds(std::vector<double>(v->data(), v->data() + N));
    if (auto v = info.lower)
      desc.set_lower_bounds(std::vector<double>(v->data(), v->data() + N));

    // Specify optional stopping criteria and tolerances
    if (info.rel_xpar_tol) desc.set_xtol_rel(*info.rel_xpar_tol);
    if (info.max_time)     desc.set_maxtime(*info.max_time);
    if (info.max_iters)    desc.set_maxeval(*info.max_iters);
    if (info.stopval)      desc.set_stopval(*info.stopval);

    // Return value
    Result<N> result;

    { // optimize
      met_trace_n("optimize");

      // Placeholder for 'x' because the library enforces std::vector :S
      std::vector<double> x(range_iter(info.x_init));

      try {
        result.code = desc.optimize(x, result.objective);
      } catch (const nlopt::roundoff_limited &e) {
        // ... fails silently for now
      } catch (const nlopt::forced_stop &e) {
        // ... fails silently for now
      } catch (const std::exception &e) {
        fmt::print("{}\n", e.what());
      }

      // Copy over potential solution to return value
      rng::copy(x, result.x.begin());
    } // optimize

    return result;
  }

  // explicit instantiation for some numbers
  template Result<wavelength_bases>   solve<wavelength_bases>(Wrapper<wavelength_bases> &); 
  template Result<wavelength_samples> solve<wavelength_samples>(Wrapper<wavelength_samples> &); 
} // namespace nlopt
