#define TRACY_NO_EXIT 1

// Metameric includes
#include <metameric/core/math.hpp>
#include <metameric/core/spectrum.hpp>
#include <metameric/core/linprog.hpp>
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
  // return eig::Array3f(1, 1, 1).matrix().normalized();
  return eig::Array3f(0.58869953, 0.12827836, -0.79810872).matrix().normalized();
  // return inv_gaussian_cdf(eig::Array3f::Random()).matrix().normalized();
} 

void solve_color_system() {
  using VT = Spec::value_type;
  using ET = CGAL::MP_Float;

  // Declare color systems i and j
  // and obtain color signal under i
  SpectralMapping mapp_i { .cmfs = models::cmfs_srgb, .illuminant = models::emitter_cie_d65 };
  SpectralMapping mapp_j { .cmfs = models::cmfs_srgb, .illuminant = models::emitter_cie_e };
  CMFS cs_i = mapp_i.finalize();
  CMFS cs_j = mapp_j.finalize();
  Colr sig_i = mapp_i.apply_color(Spec(0.5));

  // generate_metamer_boundary(mapp_i, mapp_j, sig_i, 256);

  // Cmpose the direction functional R31 -> R, with the color signal map to produce
  // a new functional R31 -> R, based on a random direction vector in R3
  auto dirv = sample_unit_sphere();
  Spec f = (cs_j.col(0) * dirv.x() + cs_j.col(1) * dirv.y() + cs_j.col(2) * dirv.z()).array();
  
  // Get the correct format for r
  // r is an m-sized vector of relations
  std::array<CGAL::Comparison_result, 3> r;
  std::ranges::fill(r, CGAL::EQUAL);

  // Get the correct format for C
  // C is an n-sized vector of constraints
  Spec C   = f;

  // Get the correct format for A
  // A is an m x n (3 x 31) matrix of constraints, where n is the number
  // of pointers to columns of size m
  std::array<VT*, wavelength_samples> A;
  eig::Matrix<float, 3, wavelength_samples> A_ = cs_i.transpose().eval();
  std::ranges::transform(A_.colwise(), A.begin(), [](auto v) { return v.data(); });

  // Get the correct format for b
  // b is an m-sized vector of values
  Colr b = sig_i;

  // Get the correct bounds for u, l, and a mask for activating these
  Spec l = Spec::Zero();
  Spec u = Spec::Ones();
  std::array<bool, wavelength_samples> m;
  std::ranges::fill(m, true);

  // Linear program type shorthands
  using LinearSolution = CGAL::Quadratic_program_solution<ET>;
  using LinearProgram  = CGAL::Linear_program_from_iterators
  <decltype(A)::value_type*,
   decltype(b)::value_type*,
   decltype(r)::value_type*,
   decltype(m)::value_type*,
   decltype(l)::value_type*,
   decltype(m)::value_type*,
   decltype(u)::value_type*,
   decltype(C)::value_type*>;

  // Construct and solve a linear program
  LinearProgram lp(wavelength_samples, /* <-- n */
                   3,                  /* <-- m */
                   A.data(),           /* n x m */
                   b.data(),           /* m x 1 */ 
                   r.data(),           /* m x 1 */
                   m.data(),           /* n x 1 */
                   l.data(),           /* n x 1 */ 
                   m.data(),           /* n x 1 */
                   u.data(),           /* n x 1 */
                   C.data(),           /* n x 1 */
                   0.f);               /* 1 x 1 */
  LinearSolution s = CGAL::solve_linear_program(lp, ET());

  // Obtain resulting variable in inaccurate spectral format
  Spec x;
  std::transform(s.variable_values_begin(), s.variable_values_end(),
    x.begin(), [](const auto &f) { return static_cast<float>(CGAL::to_double(f)); });

  std::cout << s << '\n';
  fmt::print("x  {}\n", x);
  fmt::print("si {}\n", mapp_i.apply_color(x));
  fmt::print("sj {}\n", mapp_j.apply_color(x));
  fmt::print("l  {}\n", l);
  fmt::print("u  {}\n", u);
  fmt::print("b  {}\n", b);
  fmt::print("r  {}\n", r);
  fmt::print("C  {}\n", C);
}

int main() {

  try { 
    using ET = CGAL::MP_Float;
    using VT = float;
    using CT = CGAL::Comparison_result;

    // N = 2, M = 3

    // A * x (r) b
    eig::Matrix<VT, 3, 2> A;
    A << 2, 3,
         1, 5,
         1, 0;
    eig::Vector<VT, 3> b = { 34, 45, 15 };
    std::array<CT, 3>  r = { CGAL::SMALLER, CGAL::SMALLER, CGAL::SMALLER };
    
    // min/max C^T @ x + c0
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

        // 
    SpectralMapping mapp_i { .cmfs = models::cmfs_srgb, .illuminant = models::emitter_cie_d65 };
    SpectralMapping mapp_j { .cmfs = models::cmfs_srgb, .illuminant = models::emitter_cie_e };
    Spec refl = 0.5f;
    std::vector<Spec> X = generate_metamer_boundary(mapp_i, mapp_j, refl, 256);
    for (auto &x : X) {
      fmt::print("{}\n", x);
    }

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