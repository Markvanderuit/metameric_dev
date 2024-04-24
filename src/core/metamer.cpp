#include <metameric/core/distribution.hpp>
#include <metameric/core/solver.hpp>
#include <metameric/core/metamer.hpp>
#include <metameric/core/moments.hpp>
#include <metameric/core/ranges.hpp>
#include <metameric/core/utility.hpp>
#include <oneapi/tbb/concurrent_vector.h>
#include <algorithm>
#include <execution>
#include <numbers>
#include <unordered_set>

// Use basis functions to reduce solver complexity (and color system size),
// or run solve on full spectral values, and then project result back
// into basis. The former is much faster, but introduces non-convexity
// into indirect reflectance and mismatching computations, breaking these in some cases.
constexpr static bool use_basis_direct_spectrum   = true;
constexpr static bool use_basis_direct_mismatch   = true;
constexpr static bool use_basis_indirect_spectrum = false;
constexpr static bool use_basis_indirect_mismatch = false;

namespace met {
  namespace detail {
    // Given a random vector in RN bounded to [-1, 1], return a vector
    // distributed over a gaussian distribution
    template <uint N>
    inline auto inv_gaussian_cdf(const eig::Array<float, N, 1> &x) {
      auto y = (-(x * x) + 1.f).max(.0001f).log().eval();
      auto z = (0.5f * y + (2.f / std::numbers::pi_v<float>)).eval();
      return (((z * z - y).sqrt() - z).sqrt() * x.sign()).eval();
    }
    
    // Given a random vector in RN bounded to [-1, 1], return a uniformly
    // distributed point on the unit sphere
    template <uint N>
    inline auto inv_unit_sphere_cdf(const eig::Array<float, N, 1> &x) {
      return inv_gaussian_cdf(x).matrix().normalized().array().eval();
    }
    
    // Generate a set of random, uniformly distributed unit vectors in RN
    template <uint N>
    inline auto gen_unit_dirs(uint n_samples, uint seed = 4) {
      met_trace();

      std::vector<eig::Array<float, N, 1>> unit_dirs(n_samples);

      if (n_samples <= 16) {
        UniformSampler sampler(-1.f, 1.f, seed);
        for (int i = 0; i < unit_dirs.size(); ++i)
          unit_dirs[i] = inv_unit_sphere_cdf(sampler.next_nd<N>());
      } else {
        UniformSampler sampler(-1.f, 1.f, seed);
        #pragma omp parallel
        { // Draw samples for this thread's range with separate sampler per thread
          UniformSampler sampler(-1.f, 1.f, seed + static_cast<uint>(omp_get_thread_num()));
          #pragma omp for
          for (int i = 0; i < unit_dirs.size(); ++i)
            unit_dirs[i] = inv_unit_sphere_cdf(sampler.next_nd<N>());
        }
      }

      return unit_dirs;
    }
  } // namespace detail

  Basis::vec_type generate_spectrum_coeffs(const DirectSpectrumInfo &info) {
    met_trace();
    
    // Take a grayscale spectrum as mean to build around
    Spec mean = Spec(luminance(info.direct_constraints[0].second)).cwiseMin(1.f);
    
    if constexpr (use_basis_direct_spectrum) {
      // Solver settings
      opt::Wrapper<wavelength_bases> solver = {
        .x_init       = 0.05,
        .upper        = 1.0,
        .lower        =-1.0,
        .max_iters    = 256,  // Failsafe
        .rel_xpar_tol = 1e-3, // Threshold for objective error
      };
      
      // Objective function minimizes l2 norm as a simple way to get relatively smooth coeffs
      solver.objective = opt::func_norm<wavelength_bases>(info.basis.func, mean);

      // Add color system equality constraints, upholding spectral metamerism
      for (const auto [csys, colr] : info.direct_constraints) {
        auto A = (csys.finalize(false).transpose() * info.basis.func).eval();
        auto b = lrgb_to_xyz(colr);

        solver.eq_constraints.push_back({ .f   = opt::func_norm<wavelength_bases>(A, b), 
                                          .tol = 1e-4 });
      }

      // Add boundary inequality constraints, upholding spectral 0 <= x <= 1
      {
        auto A = (eig::Matrix<float, 2 * wavelength_samples, wavelength_bases>()
          << info.basis.func, -info.basis.func).finished();
        auto b = (eig::Array<float, 2 * wavelength_samples, 1>()
          << Spec(1.f), Spec(0.f)).finished();
        solver.nq_constraints_v.push_back({ .f   = opt::func_dot_v<wavelength_bases>(A, b), 
                                            .n   = 2 * wavelength_samples, 
                                            .tol = 1e-2 });
      }

      // Run solver and return recovered coefficients
      return solve(solver).x.cast<float>().eval();
    } else {
      // Solver settings
      opt::Wrapper<wavelength_samples> solver = {
        .x_init       = 0.5,
        .upper        = 1.0,
        .lower        = 0.0,
        .max_iters    = 256,
        .rel_xpar_tol = 1e-2
      };

      // Objective function minimizes l2 norm over spectral distribution
      auto obj = eig::Matrix<float, wavelength_samples, wavelength_samples>::Identity();
      solver.objective = opt::func_norm<wavelength_samples>(obj, mean);

      // Add color system equality constraints, upholding spectral metamerism
      for (const auto [csys, colr] : info.direct_constraints) {
        auto A = csys.finalize(false).transpose().eval();
        auto b = lrgb_to_xyz(colr);
        solver.eq_constraints.push_back({ .f   = opt::func_norm<wavelength_samples>(A, b), 
                                          .tol = 1e-4 });
      }
      
      // Run solver and return recovered spectral distribution
      auto r = solve(solver).x.cast<float>().array().cwiseMax(0.f).cwiseMin(1.f).eval();
      return generate_spectrum_coeffs(SpectrumCoeffsInfo { .spec = r, .basis = info.basis });
    }
  }

  Basis::vec_type generate_spectrum_coeffs(const IndirectSpectrumInfo &info) {
    met_trace();
    
    // Take a grayscale spectrum as mean to build around
    Spec mean = Spec(luminance(info.direct_constraints[0].second)).cwiseMin(1.f);

    if constexpr (use_basis_indirect_spectrum) {
      // Solver settings
      opt::Wrapper<wavelength_bases> solver = {
        .x_init       = 0.05,
        .upper        = 1.0,
        .lower        =-1.0,
        .max_iters    = 128,  // Failsafe
        .rel_xpar_tol = 1e-3, // Threshold for objective error
      };

      // Objective function minimizes l2 norm over spectral distribution
      solver.objective = opt::func_norm<wavelength_bases>(info.basis.func, mean);

      // Add boundary inequality constraints, upholding spectral 0 <= x <= 1
      {
        auto A = (eig::Matrix<float, 2 * wavelength_samples, wavelength_bases>()
          << info.basis.func, -info.basis.func).finished();
        auto b = (eig::Array<float, 2 * wavelength_samples, 1>()
          << Spec(1.f), Spec(0.f)).finished();
        solver.nq_constraints_v.push_back({ .f   = opt::func_dot_v<wavelength_bases>(A, b),
                                            .n   = 2 * wavelength_samples, 
                                            .tol = 1e-2 });
      }

      // Add color system equality constraints, upholding spectral metamerism
      for (const auto [csys, colr] : info.direct_constraints) {
        auto A = (csys.finalize(false).transpose() * info.basis.func).eval();
        auto b = lrgb_to_xyz(colr);
        solver.eq_constraints.push_back({ .f   = opt::func_norm<wavelength_bases>(A, b), 
                                          .tol = 1e-2 });
      }

      // Add interreflection equality constraint, upholding requested output color;
      // specify three equalities for three partial derivatives
      for (const auto [csys, colr] : info.indirect_constraints) {
        using vec = opt::Wrapper<wavelength_bases>::vec;
        auto A_ = csys.finalize(false);
        auto b_ = lrgb_to_xyz(colr);

        {
          using vec = eig::Vector<ad::real1st, wavelength_bases>;
          solver.eq_constraints.push_back({
          .f   = ad::wrap_capture<wavelength_bases>(
          [A = A_
                | vws::transform([](const CMFS &cmfs) { 
                    return cmfs.transpose().cast<double>().eval(); })
                | rng::to<std::vector>(), 
          B = info.basis.func.matrix().cast<double>().eval(), 
          b = b_.cast<double>().eval()](const vec &x) {
            // Recover spectral distribution
            auto r = (B * x.matrix()).eval();

            // f(x) = ||Ax - b||; A(x) = a_0 + a_1*(Bx) + a_2*(Bx)^2 + a_3*(Bx)^3 + ..
            auto Ax = (A[0].rowwise().sum() + A[1] * r).eval();
            for (uint i = 2; i < A.size(); ++i) {
              r.array() *= r.array();
              Ax        += A[i] * r;
            }
            return (Ax.array() - b).matrix().norm();
          }), .tol = 1e-2 });
        }
      }

      return solve(solver).x.cast<float>().eval();
    } else {
      // Solver settings
      opt::Wrapper<wavelength_samples> solver = {
        .x_init       = 0.5,
        .upper        = 1.0,
        .lower        = 0.0,
        .max_iters    = 128,  // Failsafe
        .rel_xpar_tol = 1e-3, // Threshold for objective error
      };

      // Objective function minimizes l2 norm over spectral distribution
      auto obj = eig::Matrix<float, wavelength_samples, wavelength_samples>::Identity();
      solver.objective = opt::func_norm<wavelength_samples>(obj, mean);

      // Add color system equality constraints, upholding spectral metamerism
      for (const auto [csys, colr] : info.direct_constraints) {
        auto A = csys.finalize(false).transpose().eval();
        auto b = lrgb_to_xyz(colr);
        solver.eq_constraints.push_back({ .f   = opt::func_norm<wavelength_samples>(A, b), 
                                          .tol = 1e-2 });
      }
      
      // Add interreflection equality constraint, upholding requested output color;
      // specify three equalities for three partial derivatives
      for (const auto [csys, colr] : info.indirect_constraints) {
        using vec = eig::Vector<ad::real1st, wavelength_samples>;
        auto A_ = csys.finalize(false);
        auto b_ = lrgb_to_xyz(colr);
        solver.eq_constraints.push_back({
        .f   = ad::wrap_capture<wavelength_samples>(
        [A = A_
              | vws::transform([](const CMFS &cmfs) { 
                  return cmfs.transpose().cast<double>().eval(); })
              | rng::to<std::vector>(), 
         b = b_.cast<double>().eval()](const vec &x) {
          // Recover spectral distribution
          auto r = x;

          // f(x) = ||Ax - b||; A(x) = a_0 + a_1*(Bx) + a_2*(Bx)^2 + a_3*(Bx)^3 + ..
          auto Ax = (A[0].rowwise().sum() + A[1] * r).eval();
          for (uint i = 2; i < A.size(); ++i) {
            r.array() *= r.array();
            Ax        += A[i] * r;
          }
          return (Ax.array() - b).matrix().norm();
        }), .tol = 1e-2 });
      }
      
      auto r = solve(solver).x.cast<float>().array().cwiseMax(0.f).cwiseMin(1.f).eval();
      return generate_spectrum_coeffs(SpectrumCoeffsInfo { r, info.basis });
    }
  }
  
  std::vector<Spec> generate_mismatching_ocs(const DirectMismatchingOCSInfo &info) {
    met_trace();
    
    // Sample unit vectors in 6d
    auto samples = detail::gen_unit_dirs<6>(info.n_samples, info.seed);

    // Output data structure 
    tbb::concurrent_vector<Spec> tbb_output;
    tbb_output.reserve(samples.size());

    if constexpr (use_basis_direct_mismatch) {
      // Solver settings
      opt::Wrapper<wavelength_bases> solver = {
        .dirc      = opt::direction::eMaximize,
        .x_init    = 0.05,
        .upper     = 1.0,
        .lower     =-1.0,
        .max_iters = 64
      };

      // Add boundary inequality constraints; upholding 0 <= x <= 1 for reflectances
      {
        auto A = (eig::Matrix<float, 2 * wavelength_samples, wavelength_bases>()
          << info.basis.func, -info.basis.func).finished();
        auto b = (eig::Array<float, 2 * wavelength_samples, 1>()
          << Spec(1.f), Spec(0.f)).finished();
        solver.nq_constraints_v.push_back({ .f   = opt::func_dot_v<wavelength_bases>(A, b), 
                                            .n   = 2 * wavelength_samples, 
                                            .tol = 1e-2 });
      }

      // Add direct color system equality constraints, upholding uplifting roundtrip
      for (const auto [csys, colr] : info.direct_constraints) {
        auto A = (csys.finalize(false).transpose() * info.basis.func).eval();
        auto b = lrgb_to_xyz(colr);
        solver.eq_constraints.push_back({ .f   = opt::func_norm<wavelength_bases>(A, b), 
                                          .tol = 1e-3 });
      }

      // This only works for the current configuration
      auto S = (eig::Matrix<float, wavelength_samples, 6>()
        << info.direct_objectives[0].finalize(false), 
           info.direct_objectives[1].finalize(false)).finished();
      eig::JacobiSVD<decltype(S)> svd;
      svd.compute(S, eig::ComputeFullV);
      auto U = (S * svd.matrixV() * svd.singularValues().asDiagonal().inverse()).eval();

      // Parallel solve for boundary spectra
      #pragma omp parallel
      {
        // Per thread copy of current solver parameter set
        auto local_solver = solver;

        #pragma omp for
        for (int i = 0; i < samples.size(); ++i) {
          // Define objective function: max (Uk)^T (Bx) -> max C^T Bx -> max ax
          auto a = ((U * samples[i].matrix()).transpose() * info.basis.func).transpose().eval();
          local_solver.objective = opt::func_dot<wavelength_bases>(a, 0.f);
            
          // Run solver and store recovered spectral distribution if it is legit
          auto r = solve(local_solver);
          guard_continue(!r.x.array().isNaN().any());
          tbb_output.push_back((info.basis(r.x.cast<float>().eval())));
        } // for (int i)
      }
    } else {
      // Solver settings
      opt::Wrapper<wavelength_samples> solver = {
        .x_init    = 0.5,
        .upper     = 1.0,
        .lower     = 0.0,
        .max_iters = 64
      };

      // Add direct color system equality constraints, upholding uplifting roundtrip
      for (const auto [csys, colr] : info.direct_constraints) {
        auto A = csys.finalize(false).transpose().eval();
        auto b = lrgb_to_xyz(colr);
        solver.eq_constraints.push_back({ .f   = opt::func_norm<wavelength_samples>(A, b), 
                                          .tol = 1e-3 });
      }

      // This only works for the current configuration
      auto S = (eig::Matrix<float, wavelength_samples, 6>()
        << info.direct_objectives[0].finalize(false), 
           info.direct_objectives[1].finalize(false)).finished();
      eig::JacobiSVD<decltype(S)> svd;
      svd.compute(S, eig::ComputeFullV);
      auto U = (S * svd.matrixV() * svd.singularValues().asDiagonal().inverse()).eval();

      // Parallel solve for boundary spectra
      #pragma omp parallel
      {
        // Per thread copy of current solver parameter set
        auto local_solver = solver;

        #pragma omp for
        for (int i = 0; i < samples.size(); ++i) {
          // Define objective function: max (Uk)^T (Bx) -> max C^T Bx -> max ax
          auto a = (U * samples[i].matrix()).eval();
          local_solver.objective = opt::func_dot<wavelength_samples>(a, 0.f);
            
          // Run solver and obtain recovered spectral distribution if it is legit
          auto r = solve(local_solver).x.cast<float>().array().cwiseMax(0.f).cwiseMin(1.f).eval();
          guard_continue(!r.isNaN().any());
          tbb_output.push_back(info.basis(generate_spectrum_coeffs(SpectrumCoeffsInfo { r, info.basis })));
        } // for (int i)
      }
    }

    return std::vector<Spec>(range_iter(tbb_output));
  }

  std::vector<Spec> generate_mismatching_ocs(const IndirectMismatchingOCSInfo &info) {
    met_trace();

    // Sample unit vectors in 6d
    auto samples = detail::gen_unit_dirs<6>(info.n_samples, info.seed);

    // Output data structure 
    tbb::concurrent_vector<Spec> tbb_output;
    tbb_output.reserve(samples.size());
    
    if constexpr (use_basis_indirect_mismatch) {
      // Solver settings
      opt::Wrapper<wavelength_bases> solver = {
        .x_init    = 0.05,
        .upper     = 1.0,
        .lower     =-1.0,
        .max_iters = 48
      };

      // Add boundary inequality constraints; upholding 0 <= x <= 1 for reflectances
      {
        auto A = (eig::Matrix<float, 2 * wavelength_samples, wavelength_bases>()
          << info.basis.func, -info.basis.func).finished();
        auto b = (eig::Array<float, 2 * wavelength_samples, 1>()
          << Spec(1.f), Spec(0.f)).finished();
        solver.nq_constraints_v.push_back({ .f   = opt::func_dot_v<wavelength_bases>(A, b), 
                                            .n   = 2 * wavelength_samples, 
                                            .tol = 1e-2 });
      }

      // Add direct color system equality constraints, upholding uplifting roundtrip
      for (const auto [csys, colr] : info.direct_constraints) {
        auto A = (csys.finalize(false).transpose() * info.basis.func).eval();
        auto b = lrgb_to_xyz(colr);
        solver.eq_constraints.push_back({ .f   = opt::func_norm<wavelength_bases>(A, b), 
                                          .tol = 1e-3 });
      }
      
      // Parallel solve for boundary spectra
      #pragma omp parallel 
      {
        // Per thread copy of current solver parameter set
        auto local_solver = solver;

        #pragma omp for
        for (int i = 0; i < samples.size(); ++i) {
          using namespace std::placeholders; // import _1, _2, _3
          using vec = eig::Vector<ad::real1st, wavelength_bases>;

          // Helper lambda to map along unit vector
          constexpr auto trf_by_sample = [](const CMFS &cmfs, const eig::Vector3f &sample) {
            return (cmfs * sample).cast<double>().eval();  };

          // Map color systems along unit vector
          auto A_direct = trf_by_sample(info.direct_objective.finalize(false), samples[i].head<3>());
          auto A_indrct = info.indirect_objective.finalize(false)
                        | vws::transform(std::bind(trf_by_sample, _1, samples[i].tail<3>().eval()))              
                        | rng::to<std::vector>();

          local_solver.objective = ad::wrap_capture<wavelength_bases>(
          [A_direct, A_indrct, B = info.basis.func.cast<double>().eval()]
          (const vec &x) {
            auto r = (B * x.matrix()).eval();
            auto diff = A_direct.dot(r) + A_indrct[0].sum() + A_indrct[1].dot(r);
            for (uint i = 2; i < A_indrct.size(); ++i) {
              r.array() *= r.array();
              diff      += A_indrct[i].dot(r);
            }
            return diff;
          });

          // Run solver and store recovered spectral distribution if it is safe
          auto r = solve(local_solver);
          guard_continue(!r.x.array().isNaN().any());
          tbb_output.push_back((info.basis(r.x.cast<float>().eval())));
        } // for (int i)
      }
    } else {
      // Solver settings
      opt::Wrapper<wavelength_samples> solver = {
        .x_init       = 0.5,
        .upper        = 1.0,
        .lower        = 0.0,
        .max_iters    = 48
      };

      // Add direct color system equality constraints, upholding uplifting roundtrip
      for (const auto [csys, colr] : info.direct_constraints) {
        auto A = csys.finalize(false).transpose().eval();
        auto b = lrgb_to_xyz(colr);
        solver.eq_constraints.push_back({ .f   = opt::func_norm<wavelength_samples>(A, b), 
                                          .tol = 1e-3 });
      }
      
      // Parallel solve for boundary spectra
      #pragma omp parallel 
      {
        // Per thread copy of current solver parameter set
        auto local_solver = solver;

        #pragma omp for
        for (int i = 0; i < samples.size(); ++i) {
          using namespace std::placeholders; // import _1, _2, _3
          using vec = eig::Vector<ad::real1st, wavelength_samples>;

          // Helper lambda to map along unit vector
          constexpr auto trf_by_sample = [](const CMFS &cmfs, const eig::Vector3f &sample) {
            return (cmfs * sample).cast<double>().eval();  };

          // Map color systems along unit vector
          auto A_direct = trf_by_sample(info.direct_objective.finalize(false), samples[i].head<3>());
          auto A_indrct = info.indirect_objective.finalize(false)
                        | vws::transform(std::bind(trf_by_sample, _1, samples[i].tail<3>().eval()))              
                        | rng::to<std::vector>();

          local_solver.objective = ad::wrap_capture<wavelength_samples>(
          [A_direct, A_indrct]
          (const vec &x) {
            auto r = x;
            auto diff = A_direct.dot(r) + A_indrct[0].sum() + A_indrct[1].dot(r);
            for (uint i = 2; i < A_indrct.size(); ++i) {
              r.array() *= r.array();
              diff      += A_indrct[i].dot(r);
            }
            return diff;
          });

          // Run solver and store recovered spectral distribution if it is legit
          auto r = solve(local_solver).x.cast<float>().array().cwiseMax(0.f).cwiseMin(1.f).eval();
          guard_continue(!r.isNaN().any());
          tbb_output.push_back(info.basis(
            generate_spectrum_coeffs(SpectrumCoeffsInfo { .spec = r, .basis = info.basis })));
        } // for (int i)
      }
    }
    
    return std::vector<Spec>(range_iter(tbb_output));
  }

  std::vector<Basis::vec_type> generate_color_system_ocs_coeffs(const DirectColorSystemOCSInfo &info) {
    met_trace();

    // Sample unit vectors in 3d
    auto samples = detail::gen_unit_dirs<3>(info.n_samples, info.seed);

    // Output for parallel solve
    tbb::concurrent_vector<Basis::vec_type> tbb_output;
    tbb_output.reserve(samples.size());

    auto A = info.direct_objective.finalize();

    // Parallel solve for boundary spectra
    #pragma omp parallel for
    for (int i = 0; i < samples.size(); ++i) {
      // Obtain actual spectrum by projecting sample onto optimal
      Spec s = (A * samples[i].matrix()).eval();
      for (auto &f : s)
        f = f >= 0.f ? 1.f : 0.f;

      // Run solve to find nearest valid spectrum within the basis function set
      auto c = generate_spectrum_coeffs(SpectrumCoeffsInfo {
        .spec  = s,
        .basis = info.basis
      });
      
      // Store valid spectrum
      guard_continue(!c.array().isNaN().any());
      tbb_output.push_back(c);
    }
    
    return std::vector<Basis::vec_type>(range_iter(tbb_output));
  }

  std::vector<Spec> generate_color_system_ocs(const DirectColorSystemOCSInfo &info) {
    met_trace();
    auto coeffs = generate_color_system_ocs_coeffs(info);
    std::vector<Spec> s(coeffs.size());
    std::transform(std::execution::par_unseq,
                   range_iter(coeffs),
                   s.begin(),
                   [&](const auto &coef) { return info.basis(coef); });
    return s;
  }

  Basis::vec_type generate_spectrum_coeffs(const SpectrumCoeffsInfo &info) {
    met_trace();
    
    opt::Wrapper<wavelength_bases> solver = {
      .x_init       = 0.05,
      .upper        = 1.0,
      .lower        =-1.0,
      .max_iters    = 128,   // Failsafe
      .rel_xpar_tol = 1e-3, // Threshold for objective error
    };

    // Objective function minimizes l2 norm over spectral distribution differences
    solver.objective = opt::func_norm<wavelength_bases>(info.basis.func, info.spec);

    // Add boundary inequality constraints, upholding spectral 0 <= x <= 1
    {
      auto A = (eig::Matrix<float, 2 * wavelength_samples, wavelength_bases>()
        << info.basis.func, -info.basis.func).finished();
      auto b = (eig::Array<float, 2 * wavelength_samples, 1>()
        << Spec(1.f), Spec(0.f)).finished();
      solver.nq_constraints_v.push_back({ .f   = opt::func_dot_v<wavelength_bases>(A, b),
                                          .n   = 2 * wavelength_samples, 
                                          .tol = 1e-2 });
    }
    
    return solve(solver).x.cast<float>().cwiseMax(-1.f).cwiseMin(1.f).eval();
  }
} // namespace met