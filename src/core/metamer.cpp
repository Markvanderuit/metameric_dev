#include <metameric/core/distribution.hpp>
#include <metameric/core/nlopt.hpp>
#include <metameric/core/metamer.hpp>
#include <metameric/core/ranges.hpp>
#include <metameric/core/utility.hpp>
#include <oneapi/tbb/concurrent_vector.h>
#include <algorithm>
#include <execution>
#include <numbers>
#include <unordered_set>

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

  Spec generate_spectrum(GenerateSpectrumInfo info) {
    met_trace();
    debug::check_expr(info.systems.size() == info.signals.size(),
      "Color system size not equal to color signal size");

    // Take a grayscale spectrum as mean to build around
    Spec mean = Spec(luminance(xyz_to_lrgb(info.signals[0]))).cwiseMin(1.f);

    // Solver settings
    NLOptInfoT<wavelength_bases> solver = {
      .algo         = NLOptAlgo::LD_SLSQP,
      .form         = NLOptForm::eMinimize,
      .x_init       = 0.5,
      .max_iters    = 256,  // Failsafe
      .rel_xpar_tol = 1e-2, // Threshold for objective error
    };

    // Objective function minimizes l2 norm over difference from mean of spectral distr.,
    // as a simple way to get relatively smooth functions
    solver.objective = detail::func_norm<wavelength_bases>(info.basis.func, Spec::Zero());

    // Add color system equality constraints, upholding spectral metamerism
    for (uint i = 0; i < info.systems.size(); ++i) {
      auto A = (info.systems[i].transpose() * info.basis.func).eval();
      auto o = (info.systems[i].transpose() * mean.matrix()).eval();
      auto b = (info.signals[i] - o.array()).eval();
      solver.eq_constraints.push_back({ .f   = detail::func_norm<wavelength_bases>(A, b), 
                                        .tol = 1e-4 });
    } // for (uint i)

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

    // Run solver and return recovered spectral distribution
    auto r = solve(solver);
    return (mean + (info.basis.func * r.x.cast<float>()).array()).cwiseMax(0.f).cwiseMin(1.f).eval();
  }

  /* Spec generate_spectrum(GenerateIndirectSpectrumInfo info) {
    met_trace();

    // Solver settings
    NLOptInfoT<wavelength_bases> solver = {
      .algo         = NLOptAlgo::LD_SLSQP,
      .form         = NLOptForm::eMinimize,
      .x_init       = 0.5,
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

    // Add color system equality constraints, upholding spectral surface metamerism
    {
      auto A = (info.base_system.transpose() * info.basis.func.matrix()).eval();
      auto b = info.base_signal;
      solver.eq_constraints.push_back({ .f   = detail::func_norm<wavelength_bases>(A, b), 
                                        .tol = 1e-3 });
    } // for (uint i)
    
    // Add interreflection equality constraint, upholding requested output color;
    // specify three equalities for three partial derivatives
    for (uint j = 0; j < 3; ++j) {
      using vec = NLOptInfoT<wavelength_bases>::vec;
      auto A = info.refl_systems
             | vws::transform([j](const CMFS &cmfs) { 
                return cmfs.col(j).transpose().cast<double>().eval(); })
             | rng::to<std::vector>();
      
      solver.eq_constraints.push_back({
        .f = [A = A, 
              B = info.basis.func.matrix().cast<double>().eval(), 
              b = static_cast<double>(info.refl_signal[j])]
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

    // Run solver and return recovered spectral distribution
    auto r = solve(solver);
    return (info.basis.func * r.x.cast<float>()).cwiseMax(0.f).cwiseMin(1.f).eval();
  } */

  Spec generate_spectrum(GenerateIndirectSpectrumInfo info) {
    met_trace();

    // Solver settings
    NLOptInfoT<wavelength_samples> solver = {
      .algo      = NLOptAlgo::LD_SLSQP,
      .form      = NLOptForm::eMinimize,
      .x_init    = 0.5,
      .upper     = 1.0,
      .lower     = 0.0,
      .max_iters = 48
    };

    // Objective function minimizes l2 norm over spectral distribution
    auto obj = eig::Matrix<float, wavelength_samples, wavelength_samples>::Identity();
    solver.objective = detail::func_norm<wavelength_samples>(obj, Spec::Zero());

    // Add color system equality constraints, upholding spectral surface metamerism
    {
      auto A = info.base_system.transpose().eval();
      auto b = info.base_signal;
      solver.eq_constraints.push_back({ .f   = detail::func_norm<wavelength_samples>(A, b), 
                                        .tol = 1e-3 });
    } // for (uint i)
    
    // Add interreflection equality constraint, upholding requested output color;
    // specify three equalities for three partial derivatives
    for (uint j = 0; j < 3; ++j) {
      using vec = NLOptInfoT<wavelength_samples>::vec;
      auto A = info.refl_systems
             | vws::transform([j](const CMFS &cmfs) { 
                return cmfs.col(j).transpose().cast<double>().eval(); })
             | rng::to<std::vector>();
      
      solver.eq_constraints.push_back({
        .f = [A = A, 
              b = static_cast<double>(info.refl_signal[j])]
        (eig::Map<const vec> x, eig::Map<vec> g) {
          // f(x) = ||Ax - b||
          // -> a_0 + a_1*(Bx) + a_2*(Bx)^2 + a_3*(Bx)^3 + ... - b
          double diff = A[0].sum();
          for (uint i = 1; i < A.size(); ++i) {
            double p = static_cast<double>(i);
            auto xp  = x.array().pow(p).matrix().eval();
            diff += A[i] * xp;
          }
          diff -= b;

          double norm = diff; // hoboy
          
          // g(x) = ||Ax - b||
          // -> (B^T*A_1*(Bx)^0 + B^T*2*A_2*(Bx) + B^T*3*a_3*(Bx)^2 + ...)
          vec grad = 0.0;
          for (uint i = 1; i < A.size(); ++i) {
            double p = static_cast<double>(i);
            auto xp  = x.array().pow(p - 1.0).matrix().eval();
            grad += p
                  * A[i].transpose().cwiseProduct(xp);
          }

          if (g.data())
            g = grad;

          return norm;
      }, .tol = 1e-3 });
    } // for (uint j)

    // Run solver and return recovered spectral distribution
    auto r = solve(solver);
    return r.x.cast<float>().cwiseMax(0.f).cwiseMin(1.f).eval();
  }

  std::vector<Spec> generate_mismatching_ocs(const GenerateMismatchingOCSInfo &info) {
    met_trace();

    // Sample unit vectors in 3d
    auto samples = detail::gen_unit_dirs<6>(info.n_samples, info.seed);

    // Solver settings
    NLOptInfoT<wavelength_bases> solver = {
      .algo      = NLOptAlgo::LD_SLSQP,
      .form      = NLOptForm::eMaximize,
      .x_init    = 0.5,
      .max_iters = 32
    };

    // This only works for the current configuration
    auto S = (eig::Matrix<float, wavelength_samples, 6>()
      << info.systems_i[0], info.systems_j[0]).finished();
    eig::JacobiSVD<decltype(S)> svd;
    svd.compute(S, eig::ComputeFullV);
    auto U = (S * svd.matrixV() * svd.singularValues().asDiagonal().inverse()).eval();

    // Add color system equality constraints, upholding spectral metamerism
    for (uint i = 0; i < info.systems_i.size(); ++i) {
      auto A = (info.systems_i[i].transpose() * info.basis.func).eval();
      auto b = info.signals_i[i];
      solver.eq_constraints.push_back({ .f   = detail::func_norm<wavelength_bases>(A, b), 
                                        .tol = 1e-4 });
    } // for (uint i)

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

    // Parallel solve for boundary spectra
    tbb::concurrent_vector<Spec> tbb_output;
    tbb_output.reserve(samples.size());
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

    return std::vector<Spec>(range_iter(tbb_output));
  }

  /* std::vector<Spec> generate_mismatching_ocs(const GenerateIndirectMismatchingOCSInfo &info) {
    met_trace();

    // Sample unit vectors in 6d
    auto samples = detail::gen_unit_dirs<6>(info.n_samples, info.seed);

    // Solver settings
    NLOptInfoT<wavelength_bases> solver = {
      .algo      = NLOptAlgo::LD_SLSQP,
      .form      = NLOptForm::eMinimize,
      .x_init    = 0.5,
      .max_iters = 64
    };

    // Add color system equality constraints, upholding spectral metamerism
    for (uint i = 0; i < info.systems_i.size(); ++i) {
      auto A = (info.systems_i[i].transpose() * info.basis.func.matrix()).eval();
      auto b = info.signals_i[i];
      solver.eq_constraints.push_back({ .f   = detail::func_norm<wavelength_bases>(A, b), 
                                        .tol = 1e-4 });
    } // for (uint i)

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

    // Parallel solve for boundary spectra
    tbb::concurrent_vector<Spec> tbb_output;
    tbb_output.reserve(samples.size());
    #pragma omp parallel
    {
      // Per thread copy of current solver parameter set
      auto local_solver = solver;

      #pragma omp for
      for (int i = 0; i < samples.size(); ++i) {
        using namespace std::placeholders; // import _1, _2, _3
        using vec = NLOptInfoT<wavelength_bases>::vec;
        
        constexpr auto trf_by_sample = [](const CMFS &cmfs, const eig::Vector3f &sample) {
          return (cmfs * sample).cast<double>().eval();  };

        // Indirect color system powers, mapped along unit vector
        auto A = info.systems_j
               | vws::transform(std::bind(trf_by_sample, _1, samples[i].head<3>().eval()))              
               | rng::to<std::vector>();

        // Direct color system, mapped along unit vector
        auto A_ = trf_by_sample(info.systems_i[0], samples[i].tail<3>());

        local_solver.objective = 
          [A, A_, B = info.basis.func.cast<double>().eval()]
          (eig::Map<const vec> x, eig::Map<vec> g) -> double {
            // Recover spectral distribution
            auto r = (B * x).eval();

            double diff = A[0].sum() + A_.dot(r);
            vec grad = B.transpose() * A_;
            for (uint i = 1; i < A.size(); ++i) {
              double p = static_cast<double>(i);

              auto fr = r.array().pow(p).matrix().eval();
              auto dr = r.array().pow(p - 1.0).matrix().eval();
              
              diff += A[i].dot(fr);
              grad += p * B.transpose() * A[i].cwiseProduct(dr).eval();
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

    return std::vector<Spec>(range_iter(tbb_output));
  } */
  

  std::vector<Spec> generate_mismatching_ocs(const GenerateIndirectMismatchingOCSInfo &info) {
    met_trace();

    // Sample unit vectors in 6d
    auto samples = detail::gen_unit_dirs<6>(info.n_samples, info.seed);

    // Solver settings
    NLOptInfoT<wavelength_samples> solver = {
      .algo         = NLOptAlgo::LD_SLSQP,
      .form         = NLOptForm::eMinimize,
      .x_init       = 0.5,
      .upper        = 1.0,
      .lower        = 0.0,
      .max_iters    = 48
    };

    // Add color system equality constraints, upholding spectral metamerism
    for (uint i = 0; i < info.systems_i.size(); ++i) {
      auto A = info.systems_i[i].transpose().eval();
      auto b = info.signals_i[i];
      solver.eq_constraints.push_back({ .f   = detail::func_norm<wavelength_samples>(A, b), 
                                        .tol = 1e-4 });
    } // for (uint i)

    // Parallel solve for boundary spectra
    tbb::concurrent_vector<Spec> tbb_output;
    tbb_output.reserve(samples.size());
    #pragma omp parallel
    {
      // Per thread copy of current solver parameter set
      auto local_solver = solver;

      #pragma omp for
      for (int i = 0; i < samples.size(); ++i) {
        using namespace std::placeholders; // import _1, _2, _3
        using vec = NLOptInfoT<wavelength_samples>::vec;
        
        constexpr auto trf_by_sample = [](const CMFS &cmfs, const eig::Vector3f &sample) {
          return (cmfs * sample).cast<double>().eval();  };

        // Indirect color system powers, mapped along unit vector
        auto A = info.systems_j
               | vws::transform(std::bind(trf_by_sample, _1, samples[i].head<3>().eval())) 
               | rng::to<std::vector>();

        // Direct color system, mapped along unit vector
        auto A_ = trf_by_sample(info.systems_i[0], samples[i].tail<3>());

        local_solver.objective = 
          [A, A_]
          (eig::Map<const vec> x, eig::Map<vec> g) -> double {
            double diff = A[0].sum() + A_.dot(x);

            vec grad = A_;
            for (uint i = 1; i < A.size(); ++i) {
              double p = static_cast<double>(i);

              auto fx = x.array().pow(p).matrix().eval();
              auto dx = x.array().pow(p - 1.0).matrix().eval();
              
              diff += A[i].dot(fx);
              grad += p * A[i].cwiseProduct(dx).eval();
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

    return std::vector<Spec>(range_iter(tbb_output));
  }

  std::vector<Spec> generate_color_system_ocs(const GenerateColorSystemOCSInfo &info) {
    met_trace();

    // Sample unit vectors in 3d
    auto samples = detail::gen_unit_dirs<3>(info.n_samples, info.seed);

    // Output for parallel solve
    tbb::concurrent_vector<Spec> tbb_output;
    tbb_output.reserve(samples.size());

    // Parallel solve for boundary spectra
    #pragma omp parallel for
    for (int i = 0; i < samples.size(); ++i) {
      // Obtain actual spectrum by projecting sample onto optimal
      Spec s = (info.system * samples[i].matrix()).eval();
      for (auto &f : s)
        f = f >= 0.f ? 1.f : 0.f;

      // Run solve to find nearest valid spectrum within the basis function set
      std::vector<CMFS> systems = { info.system };
      std::vector<Colr> signals = { (info.system.transpose() * s.matrix()).eval() };
      s = generate_spectrum({  .basis = info.basis, .systems = systems, .signals = signals });

      // Store valid spectrum
      guard_continue(!s.isNaN().any());
      tbb_output.push_back(s);
    }
    
    return std::vector<Spec>(range_iter(tbb_output));
  }
} // namespace met