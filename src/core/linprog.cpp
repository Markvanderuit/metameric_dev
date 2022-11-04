#include <metameric/core/linprog.hpp>
#include <metameric/core/detail/trace.hpp>
#include <CGAL/QP_models.h>
#include <CGAL/QP_functions.h>
#include <CGAL/MP_Float.h>
#include <algorithm>
#include <execution>
#include <limits>
#include <numbers>
#include <random>
#include <ranges>
#include <omp.h>
#include <fmt/core.h>

#include <ClpSimplex.hpp>
#include <ClpLinearObjective.hpp>

namespace met {
  template <typename Ty, uint N, uint M>
  eig::Matrix<Ty, N, 1> linprog(LPParams<Ty, N, M> &params) {
    met_trace();

    // Linear program type shorthands
    using CGComp     = CGAL::Comparison_result;
    using CGFloat    = double; //CGAL::MP_Float; // grumble grumble
    using CGOptions  = CGAL::Quadratic_program_options;
    using CGSolution = CGAL::Quadratic_program_solution<CGFloat>;
    using CGProgram  = CGAL::Linear_program_from_iterators
      <Ty**, Ty*, CGComp*, bool*, Ty*, bool*, Ty*, Ty*>;
    
    // Create solver components in the correct iterable format
    std::array<Ty*, N> A;
    std::ranges::transform(params.A.colwise(), A.begin(), [](auto v) { return v.data(); });
    auto m_l = (params.l != std::numeric_limits<Ty>::min()).eval();
    auto m_u = (params.u != std::numeric_limits<Ty>::max()).eval();

    // Construct and solve linear programs for minimization/maximization
    CGProgram lp(N, M, 
      A.data(),   params.b.data(), 
      (CGComp *)  params.r.data(), 
      m_l.data(), params.l.data(), 
      m_u.data(), params.u.data(), 
      params.C.data(), 
      params.c0);
    CGSolution s = CGAL::solve_linear_program(lp, CGFloat());
      
    // Obtain and return result
    eig::Matrix<Ty, N, 1> v;
    std::transform(s.variable_values_begin(), s.variable_values_end(), v.begin(), 
      [](auto f) { return static_cast<Ty>(CGAL::to_double(f)); });
    return v;
  }
  
  template <typename Ty>
  eig::MatrixX<Ty> linprog(LPParamsX<Ty> &params) {
    met_trace();

    // Linear program type shorthands
    using CGComp     = CGAL::Comparison_result;
    using CGFloat    = double; //CGAL::MP_Float; // grumble grumble
    using CGOptions  = CGAL::Quadratic_program_options;
    using CGSolution = CGAL::Quadratic_program_solution<CGFloat>;
    using CGProgram  = CGAL::Linear_program_from_iterators
      <Ty**, Ty*, CGComp*, bool*, Ty*, bool*, Ty*, Ty*>;

    // Create solver components in the correct iterable format
    std::vector<Ty*> A(params.N);
    std::ranges::transform(params.A.colwise(), A.begin(), [](auto v) { return v.data(); });
    auto m_l = (params.l != std::numeric_limits<Ty>::min()).eval();
    auto m_u = (params.u != std::numeric_limits<Ty>::max()).eval();

    // Construct and solve linear programs for minimization/maximization
    CGProgram lp(
      params.N, params.M, 
      A.data(),   params.b.data(), 
      (CGComp *)  params.r.data(), 
      m_l.data(), params.l.data(), 
      m_u.data(), params.u.data(), 
      params.C.data(), 
      params.c0);
    // fmt::print("N = {}, M = {}\n", params.N, params.M);
    // fmt::print("Ar = {}, Ac = {}\n", params.A.rows(), params.A.cols());
    CGSolution s = CGAL::solve_linear_program(lp, CGFloat());
    
    eig::MatrixX<Ty> v(params.N, 1);
    std::transform(s.variable_values_begin(), s.variable_values_end(), v.data(), 
      [](auto f) { return static_cast<Ty>(CGAL::to_double(f)); });
    return v;
  }

  eig::ArrayXd lp_solve(const LPParameters &params) {
    // Set up a simplex model without log output
    ClpSimplex model;
    model.messageHandler()->setLogLevel(0); // shuddup you
    model.resize(0, params.N);

    // Pass in objective coefficients
    for (int i = 0; i < params.N; ++i)
      model.setObjCoeff(i, params.C[i]);

    // Pass in solution constraints
    for (int i = 0; i < params.N; ++i)
      model.setColBounds(i, -COIN_DBL_MAX, COIN_DBL_MAX);

    // Pass in constraints matrix
    eig::ArrayXi indices = eig::ArrayXi::LinSpaced(params.N, 0, params.N - 1);
    for (int i = 0; i < params.M; ++i) {
      auto values = params.A.row(i).eval();
      double b_l = (params.r[i] == LPCompare::eLE) ? -COIN_DBL_MAX : params.b[i]; 
      double b_u = (params.r[i] == LPCompare::eGE) ?  COIN_DBL_MAX : params.b[i]; 
      model.addRow(params.N, indices.data(), values.data(), b_l, b_u);
    } 

    // Solve for solution with requested method
    model.dual();
    /* if (params.method == LPMethod::ePrimal) {
      model.primal();
    } else {
      model.dual();
    } */

    // Obtain solution vector
    eig::VectorXd x(params.N);
    const double *ptr = model.getColSolution();
    std::copy(ptr, ptr + params.N, x.data());

    return x;
  }

  template <typename Ty>
  eig::MatrixX<Ty> linprog_test(LPParamsX<Ty> &params) {
    ClpSimplex model;
    model.messageHandler()->setLogLevel(0); // shuddup you
    model.resize(0, params.N);

    // Set objective coefficients
    for (int i = 0; i < params.N; ++i)
      model.setObjCoeff(i, params.C[i]);

    // Set constraint to unbounded 
    for (int i = 0; i < params.N; ++i)
      model.setColBounds(i, -COIN_DBL_MAX, COIN_DBL_MAX);

    // Set constraint data
    eig::ArrayXi row_idx = eig::ArrayXi::LinSpaced(params.N, 0, params.N - 1);
    for (int i = 0; i < params.M; ++i) {
      double b_l = (params.r[i] == LPComp::eLE) ? -COIN_DBL_MAX : params.b[i]; 
      double b_u = (params.r[i] == LPComp::eGE) ?  COIN_DBL_MAX : params.b[i]; 
      auto row_data = params.A.row(i).eval();
      model.addRow(params.N, row_idx.data(), row_data.data(), b_l, b_u);
    }

    // Solve and obtain result
    model.dual();
    
    const double * model_sol = model.getColSolution();
    eig::VectorXd result(params.N);
    std::copy(model_sol, model_sol + params.N, result.data());
    
    return result.cast<Ty>();
  }

  /* Forward declarations */
  
  template
  eig::Matrix<float, 7, 1> linprog<float, 7, 65>(LPParams<float, 7, 65>&);
  template
  eig::Matrix<float, 7, 1> linprog<float, 7, 68>(LPParams<float, 7, 68>&);
  template
  eig::Matrix<float, 10, 1> linprog<float, 10, 65>(LPParams<float, 10, 65>&);
  template
  eig::Matrix<float, 10, 1> linprog<float, 10, 68>(LPParams<float, 10, 68>&);
  template
  eig::MatrixX<float> linprog<float>(LPParamsX<float>&);
  template
  eig::MatrixX<double> linprog_test<double>(LPParamsX<double>&);
} // namespace met