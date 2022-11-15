#include <metameric/core/linprog.hpp>
#include <metameric/core/detail/trace.hpp>
#include <ClpSimplex.hpp>
#include <ClpLinearObjective.hpp>
#include <limits>
#include <ranges>


namespace met {
  LPParameters::LPParameters(uint M, uint N)
  : M(M), 
    N(N),
    C(N),
    A(M, N),
    b(M),
    r(M),
    x_l(N),
    x_u(N) {
    // Initialize sensible default values where possible
    r = LPCompare::eEQ;
    C = 1.f;
    x_u = std::numeric_limits<double>::max();
    x_l =-std::numeric_limits<double>::max();
  }


  eig::ArrayXd lp_solve(const LPParameters &params) {
    met_trace();

    // Set up a simplex model without log output
    ClpSimplex model;
    model.messageHandler()->setLogLevel(0); // shuddup you
    model.resize(0, params.N);

    // Pass in objective coefficients and constraintss
    for (int i = 0; i < params.N; ++i) {
      model.setObjCoeff(i, (params.objective == LPObjective::eMaximize) ? params.C[i] : -params.C[i]);
      model.setColBounds(i, params.x_l[i], params.x_u[i]);
    }

    // Pass in constraints matrix
    eig::ArrayXi indices = eig::ArrayXi::LinSpaced(params.N, 0, params.N - 1);
    for (int i = 0; i < params.M; ++i)
      model.addRow(params.N, 
                   indices.data(), 
                   params.A.row(i).eval().data(),
                   params.r[i] == LPCompare::eLE ? -COIN_DBL_MAX : params.b[i], 
                   params.r[i] == LPCompare::eGE ?  COIN_DBL_MAX : params.b[i]);

    // Model settings
    model.scaling(params.scaling ? 1 : 0);

    // Solve for solution with requested method
    if (params.method == LPMethod::ePrimal) {
      model.primal();
    } else {
      model.dual();
    }
    model.cleanup(3);

    // Obtain solution data and copy to return object
    std::span<const double> solution = { model.getColSolution(), params.N };
    eig::VectorXd x(params.N);
    std::ranges::copy(solution, x.begin());

    return x;
  }
} // namespace met