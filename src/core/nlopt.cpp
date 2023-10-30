#include <metameric/core/nlopt.hpp>
#include <metameric/core/utility.hpp>

namespace met {
  static uint n_objective_calls = 0;
  
  double objective(unsigned n, const double *x, double *grad, void *my_func_data) {
    n_objective_calls++;

    if (grad) {
      grad[0] = 0.0;
      grad[1] = 0.5 / sqrt(x[1]);
    }

    return sqrt(x[1]);
  }

  typedef struct {
    double a, b;
  } my_constraint_data;

  double myconstraint(unsigned n, const double *x, double *grad, void *data)
  {
    my_constraint_data *d = (my_constraint_data *) data;
    double a = d->a, b = d->b;
    if (grad) {
        grad[0] = 3 * a * (a*x[0] + b) * (a*x[0] + b);
        grad[1] = -1.0;
    }
    return ((a*x[0] + b) * (a*x[0] + b) * (a*x[0] + b) - x[1]);
  }
  
  void test_nlopt() {
    met_trace();
    nlopt::opt opt(nlopt::algorithm::LD_MMA, 2);

    opt.set_lower_bounds({ -HUGE_VAL, 0 });
    opt.set_min_objective(objective, nullptr);
    
    my_constraint_data data[2] = { {2,0}, {-1,1} };
    opt.add_inequality_constraint(myconstraint, &data[0], 1e-8);
    opt.add_inequality_constraint(myconstraint, &data[1], 1e-8);
    opt.set_xtol_rel(1e-4);

    std::vector<double> x = { 1.234, 5.678 };  /* some initial guess */
    double minf; /* the minimum objective value, upon return */
    if (auto r = opt.optimize(x, minf)) {
      fmt::print("Erorr: {}\n", r);
    }
    fmt::print("Found f{} = {} tolerance after {} iterations\n", x, minf, n_objective_calls);
  }

  eig::ArrayXd solve(const OptInfo &info) {
    met_trace();
    // ...
    return eig::ArrayXd();
  }
} // namespace met
