// Metameric includes
#include <metameric/core/math.hpp>
#include <metameric/core/spectrum.hpp>
#include <metameric/core/utility.hpp>

// STL includes
#include <algorithm>
#include <cstdlib>
#include <exception>
#include <numeric>
#include <span>
#include <string>
#include <vector>
#include <numbers>
#include <iostream>

// Oh dear
// #include <igl/linprog.h>
// #include <ClpSimplex.hpp>
// #include <metameric/core/simplex_solver.hpp>
#include <CGAL/QP_models.h>
#include <CGAL/QP_functions.h>
#include <CGAL/MP_Float.h>

using namespace met;

const float PI                 = 3.1415926538f;
const float GAUSSIAN_EPSILON   = .0001f;
const float GAUSSIAN_ALPHA     = 1.f;
const float GAUSSIAN_INV_ALPHA = 1.f / GAUSSIAN_ALPHA;
const float GAUSSIAN_K         = 2.0 / (PI * GAUSSIAN_ALPHA);

eig::Array3f inv_gaussian_cdf(const eig::Array3f &x) {
  auto y = (eig::Array3f(1.f) - x * x).max(GAUSSIAN_EPSILON).log().eval();
  auto z = eig::Array3f(GAUSSIAN_K) + 0.5f * y;
  return ((z * z - y * GAUSSIAN_INV_ALPHA).sqrt() - z).sqrt() * x.sign();
}

eig::Array3f sample_unit_sphere() {
  return inv_gaussian_cdf(eig::Array3f::Random()).matrix().normalized();
} 

int main() {
  try { 
    // program and solution types
    // using Program = CGAL::Quadratic_program<int>;
    // using Solution = CGAL::Quadratic_program_solution<CGAL::MP_Float>;

    // Program lp(CGAL::SMALLER, true, 0, false, 0);
    // const int X = 0;
    // const int Y = 1;
    // lp.set_a(X, 0, -1); lp.set_a(Y, 0, 1); lp.set_b(0, 7); //  x + y  <= 7
    // lp.set_a(X, 1, -1); lp.set_a(Y, 1, 2); lp.set_b(1, 4); // -x + 2y <= 4
    // lp.set_u(Y, true, 4);                                  //       y <= 4
    // lp.set_c(Y, -32);                                      // -32y
    // lp.set_c0(64);                                         // +64
    
/*     // by default, we have a nonnegative LP with Ax <= b
    Program lp (CGAL::SMALLER, true, 0, false, 0);
    // now set the non-default entries
    const int X = 0;
    const int Y = 1;
    
                                                            // min
    lp.set_c(Y, -32);                                       // -32y
    lp.set_c0(64);                                          // + 64
                                                            // w.r.t.
    lp.set_a(X, 0,  1); lp.set_a(Y, 0, 1); lp.set_b(0, 7);  //  1x + 1y <= 7
    lp.set_a(X, 1, -1); lp.set_a(Y, 1, 2); lp.set_b(1, 4);  // -1x + 2y <= 4
    // lp.set_u(Y, true, 4);                                   // y <= 4 */

    // CGAL::solve_nonnegative_linear_program
    // Solution s = CGAL::solve_nonnegative_linear_program(lp, CGAL::MP_Float());
    // if (s.solves_linear_program(lp)) {
    //   std::cout << s << '\n';
    // }
  
  using ET = CGAL::MP_Float;
  using VT = float;
  using CT = CGAL::Comparison_result;

  // A * x (r) b
  eig::Matrix<VT, 3, 2> A;
  A << 2, 3,
       1, 5,
       1, 0;
  eig::Vector<VT, 3> b = { 34, 45, 15 };
  std::array<CT, 3>  r = { CGAL::SMALLER, CGAL::SMALLER, CGAL::SMALLER };
  
  // min C^T @ x + c0
  eig::Vector<VT, 2> C = { -1, -2 };
  VT                c0 = 0;
  
  // Get the correct iterable format for A
  std::array<VT*, 2> A_;
  std::ranges::transform(A.colwise(), A_.begin(), [](auto v) { return v.data(); });
  
  // Linear program type shorthands
  using Solution = CGAL::Quadratic_program_solution<ET>;
  typedef CGAL::Nonnegative_linear_program_from_iterators
  <decltype(A_)::value_type*, 
   decltype(b)::value_type*, 
   decltype(r)::value_type*,
   decltype(C)::value_type*> Program;

  // Construct and solve a linear program
  Program lp(2, 3, A_.data(), b.data(), r.data(), C.data(), c0);
  Solution s = CGAL::solve_nonnegative_linear_program(lp, ET());
  if (s.solves_linear_program(lp)) std::cout << s << '\n';

    /* 
    // Declare constraints in format Ax <= b
    eig::Matrix<double, 3, 2> A;
    A << 2, 3,
         1, 5,
         1, 0;
    eig::Matrix<double, 3, 1> b;
    b << 34,
         45,
         15;
    
    // Declare objective matrix C
    eig::Matrix<double, 2, 1> C(2);
    C << 1,
         1;

    // Adhere to dynamic size format
    eig::MatrixXd constraints(3, 3);
    constraints << A, b;
    eig::MatrixXd objective(C);

    // Run solver
    SimplexSolver solver(SIMPLEX_MAXIMIZE, objective, constraints); */
    
    /* // Output solution
    fmt::print("Has solution: {}\n", solver.hasSolution());
    if (solver.hasSolution()) {
      fmt::print("Maximum     : {}\n", solver.getOptimum());
      fmt::print("Solution    : {}\n", solver.getSolution());
    } */

    /* // Declare color system i
    SpectralMapping mapp_i { .cmfs = models::cmfs_srgb, .illuminant = models::emitter_cie_d65 };
    CMFS            cs_i = mapp_i.finalize();

    // Declare color system j
    SpectralMapping mapp_j { .cmfs = models::cmfs_srgb, .illuminant = models::emitter_cie_e };
    CMFS            cs_j = mapp_j.finalize();

    // Declare observer color signal under i
    Colr sig_i = cs_i.matrix().transpose() * Spec(0.5).matrix();

    // Sample a random direction vector in R3
    auto dirv = sample_unit_sphere();

    // Compose functional R31 -> R as dot(cs_j, dirv)
    Spec jF =(cs_j.col(0) * dirv.x()
            + cs_j.col(1) * dirv.y()
            + cs_j.col(2) * dirv.z()).array(); */
    
    // Construct a linear programming problem of the form
    // 
    // max_x/min_x jF' x
    //  cs_i' x = sig_i
    //  0 <= x <= 1
    // 
    // incorrectly rewritten without slack variables
    // 
    // max_x jF' x
    // 
    //   cs_i'x <= sig_i
    //   x      <= 1
    //   x      >= 0 (implicit, not included)

    /* using WvlConstr = eig::Matrix<float, wavelength_samples, wavelength_samples>;

    eig::VectorXf sv_objective(jF);

    eig::MatrixXf sv_constraints(3 + wavelength_samples, wavelength_samples + 1);

    sv_constraints << cs_i.transpose().eval(),      sig_i,
                      WvlConstr::Identity().eval(), Spec(1);

    std::cout << sv_constraints << std::endl;

    // Run solver
    SimplexSolver sv_solver(SIMPLEX_MAXIMIZE, sv_objective.cast<double>(), sv_constraints.cast<double>());
    
    // Output solution if exists
    fmt::print("Has solution: {}\n", sv_solver.hasSolution());
    if (sv_solver.hasSolution()) {
      Spec s = sv_solver.getSolution().cast<float>();

      fmt::print("Maximum     : {}\n", sv_solver.getOptimum());
      fmt::print("Solution    : {}\n", s);

      fmt::print("In        : {}\n", sig_i);
      fmt::print("Out       : {}\n", (cs_i.matrix().transpose() * s.matrix()).eval());
    } */

    // std::cout << "C  = " << sv_objective << std::endl;
    // std::cout << "Ab = " << sv_constraints.transpose().eval() << std::endl;

    /* // Define color system
    SpectralMapping mapp {
      .cmfs       = models::cmfs_srgb,
      .illuminant = models::emitter_cie_d65,
    };
    
    // System of the form A'x = b, 0 <= x <= 1 
    // eig::Array<float, 31, 3> A;
    // eig::Array<float, 3, 1>  b;
    // auto x = A.matrix().llt().solve(b.matrix());

    Spec r_i = 0.5f;
    CMFS cs  = mapp.finalize();  // Color system spectra
    Colr v_i = cs.matrix().transpose() * r_i.matrix();

    auto A = cs.matrix().transpose().eval();
    auto b = v_i.matrix().eval();
    
    auto g = eig::LeastSquaresConjugateGradient<decltype(A)>();
    g.compute(A);
    
    auto x = g.solve(b).array().max(0.f).min(1.f);
    Colr v_j = cs.matrix().transpose() * x.matrix();

    fmt::print("i = {}\n", g.iterations()); 
    fmt::print("{} -> {}\n", v_i, v_j); 
    fmt::print("x = {}\n", x);  */

   /*  auto rn = sample_unit_sphere();
    auto c_ =(cs.col(0) * rn.x()
            + cs.col(1) * rn.y()
            + cs.col(2) * rn.z()).eval();

    auto c = eig::VectorXd(c_.cast<double>());
    auto A = eig::MatrixXd(cs.matrix().cast<double>());
    auto b = eig::VectorXd(v_i.matrix().cast<double>());

    std::cout << "c = " << c << std::endl;
    std::cout << "A = " << A << std::endl;
    std::cout << "b = " << b << std::endl;

    eig::VectorXd x(r_i.matrix().cast<double>());

    igl::linprog(c, A, b, 16, x);

    Spec r_j = x.cast<float>();
    Colr v_j = cs.matrix().transpose() * r_j.matrix();

    // Colr b = 0.5f;             // Neutral gray result
    // auto x = A.transpose().matrix().colPivHouseholderQr().solve(b.matrix());
*/

    fmt::print("Reached end successfully\n");
  } catch (const std::exception &e) {
    fmt::print(stderr, "{}", e.what());
    return EXIT_FAILURE;
  }
  return EXIT_SUCCESS;
}