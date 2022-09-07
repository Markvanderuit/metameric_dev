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
    template <uint N>
    eig::Array<float, N, 1> inv_gaussian_cdf(const eig::Array<float, N, 1> &x) {
      using ArrayNf = eig::Array<float, N, 1>;
      met_trace();

      auto y = (ArrayNf(1.f) - x * x).max(gaussian_epsilon).log().eval();
      auto z = (ArrayNf(gaussian_k) + 0.5f * y).eval();
      return (((z * z - y * gaussian_alpha_inv).sqrt() - z).sqrt() * x.sign()).eval();
    }
    
    // Given a random vector in R3 bounded to [-1, 1], return a uniformly
    // distributed point on the unit sphere
    template <uint N>
    eig::Array<float, N, 1> inv_unit_sphere_cdf(const eig::Array<float, N, 1> &x) {
      met_trace();
      return inv_gaussian_cdf<N>(x).matrix().normalized().eval();
    }

    template <uint N>
    std::vector<eig::Array<float, N, 1>> generate_unit_dirs(uint n_samples) {
      using ArrayNf = eig::Array<float, N, 1>;
      met_trace();

      // Generate separate seeds for each thread's rng
      std::random_device rd;
      using SeedTy = std::random_device::result_type;
      std::vector<SeedTy> seeds(omp_get_max_threads());
      for (auto &s : seeds) s = rd();

      std::vector<ArrayNf> unit_dirs(n_samples);
      #pragma omp parallel
      {
        // Initialize separate random number generator per thread
        std::mt19937 eng(seeds[omp_get_thread_num()]);
        std::uniform_real_distribution<float> distr(-1.f, 1.f);

        // Draw samples for this thread's range
        #pragma omp for
        for (int i = 0; i < unit_dirs.size(); ++i) {
          ArrayNf v;
          for (auto &f : v) f = distr(eng);

          unit_dirs[i] = detail::inv_unit_sphere_cdf<N>(v);
        }
      }

      return unit_dirs;
    }

    template <typename Ty, uint N, uint M>
    struct LinprogParams {
      // Components for defining "min C^T x + c0, w.r.t. Ax <=> b"
      eig::Array<Ty, N, 1> C;
      eig::Array<Ty, M, N> A;
      eig::Array<Ty, M, 1> b;
      Ty                   c0;

      // Relation for Ax <=> b (<=, ==, >=)
      eig::Array<CGAL::Sign, M, 1> r = CGAL::EQUAL;

      // Upper/lower limits to x, and masks to activate/deactivate them
      eig::Array<Ty, N, 1> l     = std::numeric_limits<Ty>::min();
      eig::Array<Ty, N, 1> u     = std::numeric_limits<Ty>::max();
      eig::Array<bool, N, 1> m_l = false;
      eig::Array<bool, N, 1> m_u = false;
    };

    template <typename Ty, uint N, uint M>
    eig::Matrix<Ty, N, 1> linprog(LinprogParams<Ty, N, M> &params) {
      met_trace();

      // Linear program type shorthands
      using LinearCompr    = CGAL::Comparison_result;
      using LinearFloat    = double; //CGAL::MP_Float; // grumble grumble
      using LinearOptions  = CGAL::Quadratic_program_options;
      using LinearSolution = CGAL::Quadratic_program_solution<LinearFloat>;
      using LinearProgram  = CGAL::Linear_program_from_iterators
        <Ty**, Ty*, LinearCompr*, bool*, Ty*, bool*, Ty*, Ty*>;

      // Create solver component A in the correct iterable format
      std::array<Ty*, N> A;
      std::ranges::transform(params.A.colwise(), A.begin(), [](auto v) { return v.data(); });

      // Construct and solve linear programs for minimization/maximization
      LinearProgram lp(N, M, A.data(), params.b.data(), params.r.data(), params.m_l.data(), 
        params.l.data(), params.m_u.data(), params.u.data(), params.C.data(), params.c0);
      LinearSolution s = CGAL::solve_linear_program(lp, LinearFloat());
      
      // Obtain and return result
      eig::Matrix<Ty, N, 1> v;
      std::transform(s.variable_values_begin(), s.variable_values_end(), v.begin(), 
        [](auto f) { return static_cast<Ty>(CGAL::to_double(f)); });
      return v;
    }

    template <typename Ty, uint N, uint M>
    eig::Matrix<Ty, N, 1> linprog(const eig::Array<Ty, N, 1> &C,
                                  const eig::Array<Ty, M, N> &A,
                                  const eig::Array<Ty, M, 1> &b,
                                  const eig::Array<Ty, N, 1> &l = std::numeric_limits<Ty>::min(),
                                  const eig::Array<Ty, N, 1> &u = std::numeric_limits<Ty>::max()) {
      met_trace();
      LinprogParams<Ty, N, M> params = { .C = C, .A = A, .b = b, .l = l, .u = u };
      params.m_l = (params.l != std::numeric_limits<Ty>::min());
      params.m_u = (params.u != std::numeric_limits<Ty>::max());
      return linprog<Ty, N, M>(params);
    }
  } // namespace detail

  std::vector<Spec> generate_metamer_boundary(const CMFS &csys_i,
                                              const CMFS &csys_j,
                                              const Colr &csig_i,
                                              const std::vector<eig::Array<float, 6, 1>> &samples) {
    using SMatrixTy = eig::Matrix<float, wavelength_samples, 6>;
    met_trace();

    // Return object
    std::vector<Spec> spectra(samples.size());

    // Obtain orthogonal spectra through SVD of dual color system matrix
    SMatrixTy S = (SMatrixTy() << csys_i, csys_j).finished();
    eig::JacobiSVD<SMatrixTy> svd(S, eig::ComputeThinU | eig::ComputeThinV);
    SMatrixTy U = (S * svd.matrixV() * svd.singularValues().asDiagonal().inverse()).eval();
    
    #pragma omp parallel
    {
      // Set all linear programming parameters (except C, which depends on our input sample)
      detail::LinprogParams<float, wavelength_samples, 3> params = {
        .A = csys_i.transpose().eval(), .b = csig_i, 
        .l = 0.f, .u = 1.f, .m_l = true, .m_u = true
      };

      #pragma omp for
      for (int i = 0; i < spectra.size(); ++i) {
        params.C = (U * samples[i].matrix()).eval();
        spectra[i] = detail::linprog<float, wavelength_samples, 3>(params);
      }
    }
                                                
    return spectra;
  }
                                              
  std::vector<Spec> generate_metamer_boundary(const CMFS &csys_i,
                                              const CMFS &csys_j,
                                              const Colr &csig_i,
                                              uint n_samples) {
    met_trace();
    return generate_metamer_boundary(csys_i, csys_j, csig_i, detail::generate_unit_dirs<6>(n_samples));
  }
  
  
  std::vector<Colr> generate_metamer_boundary_c(const CMFS &csys_i,
                                                const CMFS &csys_j,
                                                const Colr &csig_i,
                                                const std::vector<eig::Array<float, 6, 1>> &samples) {
    met_trace();

    // Generate boundary spectra
    std::vector<Spec> opt = generate_metamer_boundary(csys_i, csys_j, csig_i, samples);

    // Apply color mapping to obtain signal values
    std::vector<Colr> sig(opt.size());
    std::transform(std::execution::par_unseq, range_iter(opt), sig.begin(),
      [&](const Spec &s) { return csys_j.transpose() * s.matrix(); });
      
    return sig;
  }

  std::vector<Colr> generate_metamer_boundary_c(const CMFS &csys_i,
                                                const CMFS &csys_j,
                                                const Colr &csig_i,
                                                uint n_samples) {
    met_trace();
    
    // Generate boundary spectra
    std::vector<Spec> opt = generate_metamer_boundary(csys_i, csys_j, csig_i, n_samples);

    // Apply color mapping to obtain signal values
    std::vector<Colr> sig(opt.size());
    std::transform(std::execution::par_unseq, range_iter(opt), sig.begin(),
      [&](const Spec &s) { return csys_j.transpose() * s.matrix(); });
      
    return sig;
  }

  Spec generate_spectrum(const CMFS &csys, const Colr &csig) {
    detail::LinprogParams<float, wavelength_samples, 3> params = {
      .C = 0.f, .A = csys.transpose().eval(), .b = csig,
      .l = 0.f, .u = 1.f, .m_l = true, .m_u = true
    };
    return detail::linprog<float, wavelength_samples, 3>(params);
  }
} // namespace met