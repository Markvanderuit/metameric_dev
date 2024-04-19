#include <metameric/core/distribution.hpp>
#include <metameric/core/nlopt.hpp>
#include <metameric/core/metamer.hpp>
#include <metameric/core/moments.hpp>
#include <metameric/core/ranges.hpp>
#include <metameric/core/utility.hpp>
#include <oneapi/tbb/concurrent_vector.h>
#include <algorithm>
#include <execution>
#include <numbers>
#include <unordered_set>

constexpr static bool solve_using_basis         = true;  // Use basis functions to reduce solver complexity (and color system size)
constexpr static bool output_moment_limited_ocs = false; // Use moment conversion to reduce color system size

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

  Basis::vec_type generate_spectrum_coeffs(DirectSpectrumInfo info) {
    met_trace();

    // Take a grayscale spectrum as mean to build around
    Spec mean = info.basis.mean; // Spec(luminance(info.direct_constraints[0].second)).cwiseMin(1.f);

    // Solver settings
    NLOptInfoT<wavelength_bases> solver = {
      .algo         = NLOptAlgo::LD_SLSQP,
      .form         = NLOptForm::eMinimize,
      .x_init       = 0.5,
      .upper        = 1.0,
      .lower        =-1.0,
      .max_iters    = 256,  // Failsafe
      .rel_xpar_tol = 1e-2, // Threshold for objective error
    };
    
    // Objective function minimizes l2 norm as a simple way to get relatively coeffs
    solver.objective = detail::func_norm<wavelength_bases>(info.basis.func, Spec::Zero());

    // Add color system equality constraints, upholding spectral metamerism
    for (const auto [csys, colr] : info.direct_constraints) {
      auto A = (csys.finalize(false).transpose() * info.basis.func).eval();
      auto o = csys(mean, false);
      auto b = (lrgb_to_xyz(colr) - o).eval();
      solver.eq_constraints.push_back({ .f   = detail::func_norm<wavelength_bases>(A, b), 
                                        .tol = 1e-4 });
      using vec = NLOptInfoT<wavelength_bases>::vec;
                                        
      // solver.eq_constraints.push_back({ 
      //   .f  = [A = csys.finalize(false).transpose().cast<double>().eval(), 
      //          B = info.basis.func.cast<double>().eval(),
      //          b = b.cast<double>().eval(), 
      //          max_v = 1.0, 
      //          min_v = 0.0]
      //     (eig::Map<const vec> x, eig::Map<vec> g) {
      //       auto r    = (B * x).eval();
      //       auto mask = (r.array() >= min_v && r.array() <= max_v).eval();



      //       // // shorthands for Ax - b and ||(Ax - b)||
      //       // auto Ax   = (A * x).eval();
      //       // auto diff = (Ax.array().cwiseMax(min_v).cwiseMin(max_v) - b).matrix().eval();
      //       // auto norm = diff.norm();

      //       // // g(x) = A^T * (Ax - b) / ||(Ax - b)||
      //       // if (g.data())
      //       //   g = mask.select(A.transpose() * (diff.array() / norm).matrix(), vec(0)).eval();

      //       // // f(x) = ||(Ax - b)||
      //       // return norm;
      //   }, 
      //   .tol = 1e-4 
      // });
    }

    // Add boundary inequality constraints, upholding spectral 0 <= x <= 1
    {
      auto A = (eig::Matrix<float, 2 * wavelength_samples, wavelength_bases>()
        << info.basis.func, -info.basis.func).finished();
      auto b = (eig::Array<float, 2 * wavelength_samples, 1>()
        << Spec(1.f) - mean, mean).finished();
      solver.nq_constraints_v.push_back({ .f   = detail::func_dot_v<wavelength_bases>(A, b), 
                                          .n   = 2 * wavelength_samples, 
                                          .tol = 1e-2 });
    }

    // Run solver and return recovered coefficients
    return solve(solver).x.cast<float>().eval();
  }

  Spec generate_spectrum(DirectSpectrumInfo info) {
    met_trace();

    // Take a grayscale spectrum as mean to build around
    Spec mean = info.basis.mean; // Spec(luminance(info.direct_constraints[0].second)).cwiseMin(1.f);

    if constexpr (solve_using_basis) {
      auto x = generate_spectrum_coeffs(info);
      return (mean + info.basis(x)).cwiseMax(0.f).cwiseMin(1.f).eval();
    } else {
      // Solver settings
      NLOptInfoT<wavelength_samples> solver = {
        .algo         = NLOptAlgo::LD_SLSQP,
        .form         = NLOptForm::eMinimize,
        .x_init       = mean.cast<double>(),
        .upper        = 1.0,
        .lower        = 0.0,
        .max_iters    = 256,
        .rel_xpar_tol = 1e-2
      };

      // Objective function minimizes l2 norm over spectral distribution
      auto obj = eig::Matrix<float, wavelength_samples, wavelength_samples>::Identity();
      solver.objective = detail::func_norm<wavelength_samples>(obj, Spec::Zero());

      // Add color system equality constraints, upholding spectral metamerism
      for (const auto [csys, colr] : info.direct_constraints) {
        auto A = csys.finalize(false).transpose().eval();
        auto o = csys(mean, false);
        auto b = (lrgb_to_xyz(colr) - o).eval();
        solver.eq_constraints.push_back({ .f   = detail::func_norm<wavelength_samples>(A, b), 
                                          .tol = 1e-4 });
      }
      
      // Run solver and return recovered spectral distribution
      auto r = solve(solver);
      return (mean + r.x.cast<float>().array()).cwiseMax(0.f).cwiseMin(1.f).eval();
    }
  }

  Basis::vec_type generate_spectrum_coeffs(IndrctSpectrumInfo info) {
    met_trace();

    // Solver settings
    NLOptInfoT<wavelength_bases> solver = {
      .algo         = NLOptAlgo::LD_SLSQP,
      .form         = NLOptForm::eMinimize,
      .x_init       = 0.5,
      .upper        = 1.0,
      .lower        =-1.0,
      .max_iters    = 256,  // Failsafe
      .rel_xpar_tol = 1e-2, // Threshold for objective error
    };

    // Objective function minimizes l2 norm over spectral distribution
    solver.objective = detail::func_norm<wavelength_bases>(info.basis.func, Spec::Zero());

    // Add boundary inequality constraints, upholding spectral 0 <= x <= 1
    {
      auto A = (eig::Matrix<float, 2 * wavelength_samples, wavelength_bases>()
        << info.basis.func, -info.basis.func).finished();
      auto b = (eig::Array<float, 2 * wavelength_samples, 1>()
        << Spec(1.f), Spec(0.f)).finished();
      solver.nq_constraints_v.push_back({ .f   = detail::func_dot_v<wavelength_bases>(A, b),
                                          .n   = 2 * wavelength_samples, 
                                          .tol = 1e-2 });
    }

    // Add color system equality constraints, upholding spectral metamerism
    for (const auto [csys, colr] : info.direct_constraints) {
      auto A = (csys.finalize(false).transpose() * info.basis.func).eval();
      auto b = lrgb_to_xyz(colr);
      solver.eq_constraints.push_back({ .f   = detail::func_norm<wavelength_bases>(A, b), 
                                        .tol = 1e-4 });
    }

    // Add interreflection equality constraint, upholding requested output color;
    // specify three equalities for three partial derivatives
    for (const auto [csys, colr] : info.indirect_constraints) {
      using vec = NLOptInfoT<wavelength_bases>::vec;
      auto A_ = csys.finalize(false);
      auto b_ = lrgb_to_xyz(colr);

      for (uint j = 0; j < 3; ++j) {
        auto A = A_
              | vws::transform([j](const CMFS &cmfs) { 
                  return cmfs.col(j).transpose().cast<double>().eval(); })
              | rng::to<std::vector>();
        solver.eq_constraints.push_back({
          .f = [A = A, 
                B = info.basis.func.matrix().cast<double>().eval(), 
                b = b_[j]]
          (eig::Map<const vec> x, eig::Map<vec> g) {
            // Recover spectral distribution
            auto r = (B * x).eval();
            
            // f(x) = ||Ax - b||
            // -> a_0 + a_1*(Bx) + a_2*(Bx)^2 + a_3*(Bx)^3 + ... - b
            double diff = A[0].sum();
            for (uint i = 1; i < A.size(); ++i) {
              double p = static_cast<double>(i);
              auto rp  = r.array().pow(p).matrix().eval();
              diff += A[i] * rp;
            }
            diff -= b;

            double norm = diff; // hoboy
            
            // g(x) = ||Ax - b||
            // -> (B^T*A_1*(Bx)^0 + B^T*2*A_2*(Bx) + B^T*3*a_3*(Bx)^2 + ...)
            vec grad = 0.0;
            for (uint i = 1; i < A.size(); ++i) {
              double p = static_cast<double>(i);
              auto rp  = r.array().pow(p - 1.0).matrix().eval();
              grad += p
                    * B.transpose()
                    * A[i].transpose().cwiseProduct(rp);
            }

            if (g.data())
              g = grad;

            return norm;
        }, .tol = 1e-3 });
      } // for (uint j)
    }

    return solve(solver).x.cast<float>().eval();
  }

  Spec generate_spectrum(IndrctSpectrumInfo info) {
    met_trace();
    auto x = generate_spectrum_coeffs(info);
    return info.basis(x).cwiseMax(0.f).cwiseMin(1.f).eval();
  }
  
  std::vector<Spec> generate_mismatching_ocs(const DirectMismatchingOCSInfo &info) {
    met_trace();
    
    // Sample unit vectors in 6d
    auto samples = detail::gen_unit_dirs<6>(info.n_samples, info.seed);

    // Output data structure 
    tbb::concurrent_vector<Spec> tbb_output;
    tbb_output.reserve(samples.size());

    if constexpr (solve_using_basis) {
      // Solver settings
      NLOptInfoT<wavelength_bases> solver = {
        .algo      = NLOptAlgo::LD_SLSQP,
        .form      = NLOptForm::eMinimize,
        .x_init    = 0.5,
        /* .upper     = 1.0,
        .lower     =-1.0, */
        .max_iters = 32
      };

      // Add boundary inequality constraints; upholding 0 <= x <= 1 for reflectances
      {
        auto A = (eig::Matrix<float, 2 * wavelength_samples, wavelength_bases>()
          << info.basis.func, -info.basis.func).finished();
        auto b = (eig::Array<float, 2 * wavelength_samples, 1>()
          << Spec(1.f), Spec(0.f)).finished();
        solver.nq_constraints_v.push_back({ .f   = detail::func_dot_v<wavelength_bases>(A, b), 
                                            .n   = 2 * wavelength_samples, 
                                            .tol = 1e-2 });
      }

      // Add direct color system equality constraints, upholding uplifting roundtrip
      for (const auto [csys, colr] : info.direct_constraints) {
        auto A = (csys.finalize(false).transpose() * info.basis.func).eval();
        auto b = lrgb_to_xyz(colr);
        solver.eq_constraints.push_back({ .f   = detail::func_norm<wavelength_bases>(A, b), 
                                          .tol = 1e-4 });
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
          local_solver.objective = detail::func_dot<wavelength_bases>(a, 0.f);
            
          // Run solver and store recovered spectral distribution if it is legit
          auto r = solve(local_solver);
          guard_continue(!r.x.array().isNaN().any());
          tbb_output.push_back((info.basis.func * r.x.cast<float>()).cwiseMax(0.f).cwiseMin(1.f).eval());
        } // for (int i)
      }
    } else {
      // Solver settings
      NLOptInfoT<wavelength_samples> solver = {
        .algo      = NLOptAlgo::LD_SLSQP,
        .form      = NLOptForm::eMinimize,
        .x_init    = 0.5,
        .upper     = 1.0,
        .lower     = 0.0,
        .max_iters = 32
      };

      // Add direct color system equality constraints, upholding uplifting roundtrip
      for (const auto [csys, colr] : info.direct_constraints) {
        auto A = csys.finalize(false).transpose().eval();
        auto b = lrgb_to_xyz(colr);
        solver.eq_constraints.push_back({ .f   = detail::func_norm<wavelength_samples>(A, b), 
                                          .tol = 1e-4 });
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
          local_solver.objective = detail::func_dot<wavelength_samples>(a, 0.f);
            
          // Run solver and store recovered spectral distribution if it is legit
          auto r = solve(local_solver);
          guard_continue(!r.x.array().isNaN().any());
          tbb_output.push_back(r.x.cast<float>().cwiseMax(0.f).cwiseMin(1.f).eval());
        } // for (int i)
      }
    }

    if constexpr (output_moment_limited_ocs) {
      return tbb_output | vws::transform(spectrum_to_moments)
                        | vws::transform(moments_to_spectrum)
                        | rng::to<std::vector>();
    } else {
      return std::vector<Spec>(range_iter(tbb_output));
    }
  }

  std::vector<Spec> generate_mismatching_ocs(const IndirectMismatchingOCSInfo &info) {
    met_trace();

    // Sample unit vectors in 6d
    auto samples = detail::gen_unit_dirs<6>(info.n_samples, info.seed);

    // Output data structure 
    tbb::concurrent_vector<Spec> tbb_output;
    tbb_output.reserve(samples.size());
    
    if constexpr (solve_using_basis) {
      // Solver settings
      NLOptInfoT<wavelength_bases> solver = {
        .algo      = NLOptAlgo::LD_SLSQP,
        .form      = NLOptForm::eMinimize,
        .x_init    = 0.5,
        /* .upper     = 1.0,
        .lower     =-1.0, */
        .max_iters = 64
      };

      // Add boundary inequality constraints; upholding 0 <= x <= 1 for reflectances
      {
        auto A = (eig::Matrix<float, 2 * wavelength_samples, wavelength_bases>()
          << info.basis.func, -info.basis.func).finished();
        auto b = (eig::Array<float, 2 * wavelength_samples, 1>()
          << Spec(1.f), Spec(0.f)).finished();
        solver.nq_constraints_v.push_back({ .f   = detail::func_dot_v<wavelength_bases>(A, b), 
                                            .n   = 2 * wavelength_samples, 
                                            .tol = 1e-2 });
      }

      // Add direct color system equality constraints, upholding uplifting roundtrip
      for (const auto [csys, colr] : info.direct_constraints) {
        auto A = (csys.finalize(false).transpose() * info.basis.func).eval();
        auto b = lrgb_to_xyz(colr);
        solver.eq_constraints.push_back({ .f   = detail::func_norm<wavelength_bases>(A, b), 
                                          .tol = 1e-4 });
      }
      
      // Parallel solve for boundary spectra
      #pragma omp parallel 
      {
        // Per thread copy of current solver parameter set
        auto local_solver = solver;

        #pragma omp for
        for (int i = 0; i < samples.size(); ++i) {
          using namespace std::placeholders; // import _1, _2, _3
          using vec = NLOptInfoT<wavelength_bases>::vec;

          // Helper lambda to map along unit vector
          constexpr auto trf_by_sample = [](const CMFS &cmfs, const eig::Vector3f &sample) {
            return (cmfs * sample).cast<double>().eval();  };

          // Map color systems along unit vector
          auto A_direct = trf_by_sample(info.direct_objective.finalize(false), samples[i].head<3>());
          auto A_indrct = info.indirect_objective.finalize(false)
                        | vws::transform(std::bind(trf_by_sample, _1, samples[i].head<3>().eval()))              
                        | rng::to<std::vector>();

          local_solver.objective =
          [A_direct, A_indrct, B = info.basis.func.cast<double>().eval()]
          (eig::Map<const vec> x, eig::Map<vec> g) -> double {
            auto r = (B * x).eval();

            double diff = A_indrct[0].sum() + A_direct.dot(r);
            vec grad = B.transpose() * A_direct;
            for (uint i = 1; i < A_indrct.size(); ++i) {
              double p = static_cast<double>(i);

              auto fr = r.array().pow(p).matrix().eval();
              auto dr = r.array().pow(p - 1.0).matrix().eval();
              
              diff += A_indrct[i].dot(fr);
              grad += p * B.transpose() * A_indrct[i].cwiseProduct(dr).eval();
            }

            if (g.data())
              g = grad;
            return diff;
          };

          // Run solver and store recovered spectral distribution if it is safe
          auto r = solve(local_solver);
          guard_continue(!r.x.array().isNaN().any());
          tbb_output.push_back((info.basis.func * r.x.cast<float>()).cwiseMax(0.f).cwiseMin(1.f).eval());
        } // for (int i)
      }
    } else {
      // Solver settings
      NLOptInfoT<wavelength_samples> solver = {
        .algo         = NLOptAlgo::LD_SLSQP,
        .form         = NLOptForm::eMinimize,
        .x_init       = 0.5,
        .upper        = 1.0,
        .lower        = 0.0,
        .max_iters    = 48
      };

      // Add direct color system equality constraints, upholding uplifting roundtrip
      for (const auto [csys, colr] : info.direct_constraints) {
        auto A = csys.finalize(false).transpose().eval();
        auto b = lrgb_to_xyz(colr);
        solver.eq_constraints.push_back({ .f   = detail::func_norm<wavelength_samples>(A, b), 
                                          .tol = 1e-4 });
      }
      
      // Parallel solve for boundary spectra
      #pragma omp parallel 
      {
        // Per thread copy of current solver parameter set
        auto local_solver = solver;

        #pragma omp for
        for (int i = 0; i < samples.size(); ++i) {
          using namespace std::placeholders; // import _1, _2, _3
          using vec = NLOptInfoT<wavelength_samples>::vec;

          // Helper lambda to map along unit vector
          constexpr auto trf_by_sample = [](const CMFS &cmfs, const eig::Vector3f &sample) {
            return (cmfs * sample).cast<double>().eval();  };

          // Map color systems along unit vector
          auto A_direct = trf_by_sample(info.direct_objective.finalize(false), samples[i].head<3>());
          auto A_indrct = info.indirect_objective.finalize(false)
                        | vws::transform(std::bind(trf_by_sample, _1, samples[i].head<3>().eval()))              
                        | rng::to<std::vector>();

          local_solver.objective =
          [A_direct, A_indrct]
          (eig::Map<const vec> r, eig::Map<vec> g) -> double {
            double diff = A_indrct[0].sum() + A_direct.dot(r);
            vec grad = A_direct;
            for (uint i = 1; i < A_indrct.size(); ++i) {
              double p = static_cast<double>(i);

              auto fr = r.array().pow(p).matrix().eval();
              auto dr = r.array().pow(p - 1.0).matrix().eval();
              
              diff += A_indrct[i].dot(fr);
              grad += p * A_indrct[i].cwiseProduct(dr).eval();
            }

            if (g.data())
              g = grad;
            return diff;
          };

          // Run solver and store recovered spectral distribution if it is safe
          auto r = solve(local_solver);
          guard_continue(!r.x.array().isNaN().any());
          tbb_output.push_back(r.x.cast<float>().cwiseMax(0.f).cwiseMin(1.f).eval());
        } // for (int i)
      }
    }
    
    if constexpr (output_moment_limited_ocs) {
      return tbb_output | vws::transform(spectrum_to_moments)
                        | vws::transform(moments_to_spectrum)
                        | rng::to<std::vector>();
    } else {
      return std::vector<Spec>(range_iter(tbb_output));
    }
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
      auto c = generate_spectrum_coeffs(DirectSpectrumInfo {
        .direct_constraints = {{ info.direct_objective, info.direct_objective(s) }},
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
} // namespace met