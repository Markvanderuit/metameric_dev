#include <metameric/core/linprog.hpp>
#include <metameric/core/detail/trace.hpp>
#include <CGAL/QP_models.h>
#include <CGAL/QP_functions.h>
#include <CGAL/MP_Float.h>
#include <algorithm>
#include <execution>
#include <random>
#include <ranges>
#include <numbers>
#include <omp.h>

namespace met {
  namespace detail {
    constexpr float gaussian_epsilon   = .0001f;
    constexpr float gaussian_alpha     = 1.f;
    constexpr float gaussian_alpha_inv = 1.f / gaussian_alpha;
    constexpr float gaussian_k         = 2.f / (std::numbers::pi_v<float> * gaussian_alpha);

    // Given a random vector in R3 bounded to [-1, 1], return a vector
    // distributed over a gaussian distribution
    eig::Array3f inv_gaussian_cdf(const eig::Array3f &x) {
      met_trace();
      auto y = (eig::Array3f::Ones() - x * x).max(gaussian_epsilon).log().eval();
      auto z = eig::Array3f(gaussian_k) + 0.5f * y;
      return ((z * z - y * gaussian_alpha_inv).sqrt() - z).sqrt() * x.sign();
    }
    
    // Given a random vector in R3 bounded to [-1, 1], return a uniformly
    // distributed point on the unit sphere
    eig::Array3f inv_unit_sphere_cdf(const eig::Array3f &x) {
      met_trace();
      auto y = inv_gaussian_cdf(x);
      return y.matrix().normalized();
    }

    std::vector<eig::Array3f> generate_unit_dirs(uint n_samples) {
      met_trace();
      std::vector<eig::Array3f> unit_dirs(n_samples);
      #pragma omp parallel
      {
        // Initialize separate random number generator per thread
        // std::random_device rd;
        std::mt19937 eng(omp_get_thread_num());
        std::uniform_real_distribution<float> distr(-1.f, 1.f);

        // Draw samples for this thread's range
        #pragma omp for
        for (int i = 0; i < unit_dirs.size(); ++i) {
          unit_dirs[i] = detail::inv_unit_sphere_cdf({ distr(eng), distr(eng), distr(eng) });
        }
      }
      return unit_dirs;
    }
  }

  std::vector<Spec> generate_metamer_boundary(const SpectralMapping &mapping_i,
                                              const SpectralMapping &mapping_j,
                                              const Spec            &reflectance,
                                              uint n_samples) {
    met_trace();
    CMFS csystem_i = mapping_i.finalize();
    CMFS csystem_j = mapping_j.finalize();
    Colr csignal_i = csystem_i.transpose() * reflectance.matrix();;
    return generate_metamer_boundary(mapping_i.finalize(), 
                                     mapping_j.finalize(), 
                                     csignal_i, 
                                     n_samples);
  }
                                              
  std::vector<Spec> generate_metamer_boundary(const CMFS &csystem_i,
                                              const CMFS &csystem_j,
                                              const Colr &csignal_i,
                                              uint n_samples) {
    met_trace();

    // Linear program type shorthands
    using LinearCompr    = CGAL::Comparison_result;
    using LinearFloat    = double; //CGAL::MP_Float; // grumble grumble
    using LinearOptions  = CGAL::Quadratic_program_options;
    using LinearSolution = CGAL::Quadratic_program_solution<LinearFloat>;
    using LinearProgram  = CGAL::Linear_program_from_iterators
      <float**, float*, LinearCompr*, bool*, float*, bool*, float*, float*>;
    
    // Obtain N randomly sampled unit vectors from the unit sphere
    std::vector<eig::Array3f> unit_dirs = detail::generate_unit_dirs(n_samples);

    // Declare solver component r; a vector of size M of equality operations
    std::array<LinearCompr, 3> r;
    std::ranges::fill(r, CGAL::EQUAL);

    // Declare solver component A; a matrix of size MxN of constraits
    std::array<float*, wavelength_samples> A;
    eig::Matrix<float, 3, wavelength_samples> A_ = csystem_i.transpose().eval();
    std::ranges::transform(A_.colwise(), A.begin(), [](auto v) { return v.data(); });

    // Declare solver component b; a vector of size M of constraint results
    Colr b = csignal_i;

    // Declare solver upper/lower bounds as [0, 1], and a mask to actiavte them
    Spec l = Spec::Zero();
    Spec u = Spec::Ones();
    std::array<bool, wavelength_samples> m;
    std::ranges::fill(m, true);

    // Specify linear program options
    LinearOptions options;
    options.set_verbosity(0);
    options.set_auto_validation(false);
    options.set_pricing_strategy(CGAL::QP_CHOOSE_DEFAULT);

    // Perform solving step for each of these in parallel
    std::vector<Spec> X(n_samples);
    std::transform(std::execution::par_unseq, range_iter(unit_dirs), X.begin(),
      [&](const eig::Array3f &unit_dir) {

      // Compose directional function R31 -> R as solver constraint
      Spec C = (csystem_j.col(0) * unit_dir[0]
             +  csystem_j.col(1) * unit_dir[1]
             +  csystem_j.col(2) * unit_dir[2]).eval();
      
      // Construct and solve a linear program
      LinearProgram lp(wavelength_samples, 3,
        A.data(), b.data(), r.data(), m.data(), 
        l.data(), m.data(), u.data(), C.data(), 0.f);
      LinearSolution s = CGAL::solve_linear_program(lp, LinearFloat(), options);

      // Obtain resulting variable in inaccurate spectral format
      Spec x;
      std::transform(s.variable_values_begin(), s.variable_values_end(), x.begin(), 
        [](const auto &f) { return static_cast<float>(CGAL::to_double(f)); });
      return x;
    });

    return X;
  }
} // namespace met