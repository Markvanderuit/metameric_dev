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
      auto z = (eig::Array3f(gaussian_k) + 0.5f * y).eval();
      return (((z * z - y * gaussian_alpha_inv).sqrt() - z).sqrt() * x.sign()).eval();
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
        std::random_device rd;
        auto seed = rd();
        std::mt19937 eng(seed);
        std::uniform_real_distribution<float> distr(-1.f, 1.f);

        // Draw samples for this thread's range
        #pragma omp for
        for (int i = 0; i < unit_dirs.size(); ++i) {
          unit_dirs[i] = detail::inv_unit_sphere_cdf(eig::Array3f { distr(eng), distr(eng), distr(eng) });
        }
      }

      return unit_dirs;
    }

    template <typename Ty, uint N, uint M>
    eig::Matrix<Ty, N, 1> linprog(eig::Array<Ty, N, 1> &C,
                                  eig::Array<Ty, M, N> &A_,
                                  eig::Array<Ty, M, 1> &b,
                                  const Ty &min_v = std::numeric_limits<Ty>::min(),
                                  const Ty &max_v = std::numeric_limits<Ty>::max()) {
      met_trace();

      // Linear program type shorthands
      using LinearCompr    = CGAL::Comparison_result;
      using LinearFloat    = double; //CGAL::MP_Float; // grumble grumble
      using LinearOptions  = CGAL::Quadratic_program_options;
      using LinearSolution = CGAL::Quadratic_program_solution<LinearFloat>;
      using LinearProgram  = CGAL::Linear_program_from_iterators
        <Ty**, Ty*, LinearCompr*, bool*, Ty*, bool*, Ty*, Ty*>;
      
      // Set equality array to describe Ax = b
      std::array<LinearCompr, M> r;
      std::ranges::fill(r, CGAL::EQUAL);

      // Determine lower/upper bounds for solver
      eig::Array<Ty, N, 1> l = min_v, u = max_v;
      eig::Array<bool, N, 1> lm = l != std::numeric_limits<Ty>::min(), 
                             um = u != std::numeric_limits<Ty>::max();

      // Create solver component A in the correct iterable format
      std::array<Ty*, N> A;
      std::ranges::transform(A_.colwise(), A.begin(), [](auto v) { return v.data(); });

      // Specify linear program options
      LinearOptions options;
      options.set_verbosity(0);
      options.set_auto_validation(false);
      options.set_pricing_strategy(CGAL::QP_CHOOSE_DEFAULT);

      // Construct and solve linear programs for minimization/maximization
      LinearProgram lp(N, M,
        A.data(),  b.data(), r.data(), lm.data(), 
        l.data(), um.data(), u.data(),  C.data(), 0.f);
      LinearSolution s = CGAL::solve_linear_program(lp, LinearFloat(), options);

      // Obtain and return result
      eig::Matrix<Ty, N, 1> v;
      std::transform(s.variable_values_begin(), s.variable_values_end(), v.begin(), 
        [](const auto &f) { return static_cast<Ty>(CGAL::to_double(f)); });
      return v;
    }
  } // namespace detail
                                              
  std::vector<Spec> generate_metamer_boundary(const CMFS &csystem_i,
                                              const CMFS &csystem_j,
                                              const Colr &csignal_i,
                                              uint n_samples) {
    met_trace();

    // Obtain N/2 randomly sampled unit vectors from the unit sphere
    std::vector<eig::Array3f> unit_dirs = detail::generate_unit_dirs(n_samples);
    return generate_metamer_boundary(csystem_i, csystem_j, csignal_i, unit_dirs);
  }

  std::vector<Spec> generate_metamer_boundary(const CMFS &csystem_i,
                                              const CMFS &csystem_j,
                                              const Colr &csignal_i,
                                              const std::vector<eig::Array3f> &samples) {
    eig::Array<float, 3, wavelength_samples> A = csystem_i.transpose().eval();
    eig::Array<float, 3, 1>                  b = csignal_i;

    // Perform solving step for N spectra in parallel
    std::vector<Spec> spectra(samples.size());
    std::transform(std::execution::par_unseq, range_iter(samples), spectra.begin(),
    [&] (const eig::Array3f &sample) {
      // Compose directional function R31 -> R as solver constraint for minimization,
      // and its negation for maximization
      Spec C = csystem_j * sample.matrix();
      
      // Perform solving step
      return detail::linprog<float, wavelength_samples, 3>(C, A, b, 0.f, 1.f);
    });
                                                
    return spectra;
  }
  
  
  std::vector<Colr> generate_metamer_boundary_c(const CMFS &csystem_i,
                                                const CMFS &csystem_j,
                                                const Colr &csignal_i,
                                                const std::vector<eig::Array3f> &samples) {
    met_trace();

    // Generate boundary spectra
    std::vector<Spec> opt = generate_metamer_boundary(csystem_i, csystem_j, csignal_i, samples);

    // Apply color mapping to obtain signal values
    std::vector<Colr> sig(opt.size());
    std::transform(std::execution::par_unseq, range_iter(opt), sig.begin(),
      [&](const Spec &s) { return csystem_j.transpose() * s.matrix(); });
      
    return sig;
  }

  std::vector<Colr> generate_metamer_boundary_c(const CMFS &csystem_i,
                                                const CMFS &csystem_j,
                                                const Colr &csignal_i,
                                                uint n_samples) {
    met_trace();
    
    // Generate boundary spectra
    std::vector<Spec> opt = generate_metamer_boundary(csystem_i, csystem_j, csignal_i, n_samples);

    // Apply color mapping to obtain signal values
    std::vector<Colr> sig(opt.size());
    std::transform(std::execution::par_unseq, range_iter(opt), sig.begin(),
      [&](const Spec &s) { return csystem_j.transpose() * s.matrix(); });
      
    return sig;
  }
} // namespace met