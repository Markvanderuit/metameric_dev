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
      //  model.setColBounds(i, params.x_l[i], params.x_u[i]);
       model.setColBounds(i, -COIN_DBL_MAX, COIN_DBL_MAX);

    // Pass in constraints matrix
    eig::ArrayXi indices = eig::ArrayXi::LinSpaced(params.N, 0, params.N - 1);
    for (int i = 0; i < params.M; ++i) {
      auto values = params.A.row(i).eval();
      auto b_l = (params.r[i] == LPCompare::eLE) ? -COIN_DBL_MAX : params.b[i]; 
      auto b_u = (params.r[i] == LPCompare::eGE) ?  COIN_DBL_MAX : params.b[i]; 
      model.addRow(params.N, indices.data(), values.data(), b_l, b_u);
    } 

    // Solve for solution with requested method
    if (params.method == LPMethod::ePrimal) {
      model.primal();
    } else {
      model.dual();
    }

    // Obtain and return solution
    eig::ArrayXd x(params.N);
    const double *solution = model.getColSolution();
    std::copy(solution, solution + params.N, x.data());
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
      //  model.setColBounds(i, params.l[i], params.u[i]);

    // Set constraint data
    eig::ArrayXi row_idx = eig::ArrayXi::LinSpaced(params.N, 0, params.N - 1);
    for (int i = 0; i < params.M; ++i) {
      // Determine bounds values
      double bound_min = -COIN_DBL_MAX, bound_max = COIN_DBL_MAX;
      switch (params.r[i]) {
        case LPComp::eEQ: bound_min = params.b[i]; bound_max = params.b[i]; break;
        case LPComp::eLE: bound_max = params.b[i]; break;
        case LPComp::eGE: bound_min = params.b[i]; break;
      }
      auto row_data = params.A.row(i).eval();
      model.addRow(params.N, row_idx.data(), row_data.data(), bound_min, bound_max);
    }
    // model.addRows()

    // Solve and obtain result
    model.dual();
    
    const double * model_sol = model.getColSolution();
    eig::VectorXd result(params.N);
    std::copy(model_sol, model_sol + params.N, result.data());

    // ClpLinearObjective obj(params.C.data(), params.N);

    
    /* // Objective - just nonzeros
    int    obj_index[] = { 0,   2   };
    double obj_value[] = { 1.0, 4.0 };

    // Upper bounds - as dense vector
    double upper[] = { 2.0, COIN_DBL_MAX, 4.0 }; */

    /*  
    ClpSimplex model;
    
    // Create space for 0 rows, 3 cols
    model.resize(0, 3);

    // Set objective coefficients
    for (int i = 0; i < 2; ++i)
      model.setObjCoeff(obj_index[i], obj_value[i]);

    // Set upper bounds
    for (int i = 0; i < 3; ++i)
      model.setColBounds(i, 0.0, upper[i]);

    // Row 1 data
    int    row_1_index[] = { 0,   2   };
    double row_1_value[] = { 1.0, 1.0 };

    // Add row 1 as >= 2.0
    model.addRow(2, row_1_index, row_1_value, 2.0, COIN_DBL_MAX);

    // Row 2 data
    int    row_2_index[] = { 0,   1,   2   };
    double row_2_value[] = { 1.0,-5.0, 1.0 };

    // Add row 2 as == 1.0
    model.addRow(3, row_2_index, row_2_value, 1.0, 1.0);
    
    // Solve
    model.dual(); */

    // [ A A A ] [x] <= [b]
    // [ A A A ] [x] <= [b]
    // [ ..... ] [.] <= [.]
    // [ A A A ] [x] <= [b]

    /* int num_rows = 10000, num_cols = 3, num_elems = num_rows * num_cols;

    ClpSimplex model;
    model.resize(num_rows, num_cols);

    std::vector<double> elements(num_elems);
    std::vector<int>    starts(num_cols);
    std::vector<int>    rows(num_elems);
    std::vector<int>    lens(num_cols);

    double * col_upper = model.columnUpper();
    double * objective = model.objective();
    double * row_lower = model.rowLower();
    double * row_upper = model.rowUpper();

    // Fill objective
    for (int i = 0; i < 2; ++i)
      objective[obj_index[i]] = obj_value[i];

    // Fill upper boundary to solution
    for (int i = 0; i < num_cols; ++i)
      col_upper[i] = upper[i];
    
    // Fill lower/upper boundary to constraint; Ax = 1
    for (int i = 0; i < num_rows; ++i) {
      row_lower[i] = 1.0;
      row_upper[i] = 1.0;
    }

    // Fill elements of constraint matrix A
    for (int i = 0; i < num_cols; ++i) {

    } */


    // model.setObjective(obj);

    // obj.resize(params.N);
    // model.resize(params.N, params.M);

    // model.setObjective(ClpO/)

    // model.addRows()

    // CoinPackedMatrix matrix(false, params.N, nullptr, params.M, nullptr);
    

    // model.addO    

    // // Vector x is Nx1, vector A is MxN
    // int nr_of_rows = params.M;
    // int nr_of_cols = params.N;

    // std::vector<CoinBigIndex>
    
    // model.addRow()
    // model.addColumn()
    // model.loadProblem()
    // model.constr
    // model.loadProblem()

    // eig::MatrixX<Ty> m(3, 3);
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