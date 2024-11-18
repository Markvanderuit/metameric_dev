#include <metameric/core/distribution.hpp>
#include <metameric/core/convex.hpp>
#include <metameric/core/solver.hpp>
#include <metameric/core/math.hpp>
#include <metameric/core/metamer.hpp>
#include <metameric/core/moments.hpp>
#include <metameric/core/ranges.hpp>
#include <metameric/core/utility.hpp>
#include <algorithm>
#include <execution>
#include <numbers>
#include <unordered_set>

namespace met {
  // Autodiff shorthands for several common eigen types
  using bvec = eig::Vector<ad::real1st, wavelength_bases>;   // basis coeff vector
  using svec = eig::Vector<ad::real1st, wavelength_samples>; // spectral coeff vector
  
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
      return inv_gaussian_cdf<N>(x).matrix().normalized().eval();
    }
    
    // Generate a set of random, uniformly distributed unit vectors in RN
    template <uint N>
    inline auto gen_unit_dirs(uint n_samples, uint seed = 4) {
      met_trace();

      std::vector<eig::Vector<float, N>> unit_dirs(n_samples);

      if (n_samples <= 16) {
        UniformSampler sampler(-1.f, 1.f, seed);
        for (int i = 0; i < unit_dirs.size(); ++i)
          unit_dirs[i] = inv_unit_sphere_cdf<N>(sampler.template next_nd<N>());
      } else {
        UniformSampler sampler(-1.f, 1.f, seed);
        #pragma omp parallel
        { // Draw samples for this thread's range with separate sampler per thread
          UniformSampler sampler(-1.f, 1.f, seed + static_cast<uint>(omp_get_thread_num()));
          #pragma omp for
          for (int i = 0; i < unit_dirs.size(); ++i)
            unit_dirs[i] = inv_unit_sphere_cdf<N>(sampler.template next_nd<N>());
        }
      }

      return unit_dirs;
    }

    // Mapping from static to dynamic, so we can vary the nr. of objectives for which samples are generated
    auto gen_unit_dirs(uint n_objectives, uint n_samples, uint seed = 4) {
      constexpr auto eig_static_to_dynamic = [](const auto &v) { return (eig::VectorXf(v.size()) << v).finished(); };
      std::vector<eig::VectorXf> X(n_samples);
      switch (n_objectives) {
        case 1:  rng::transform(gen_unit_dirs<3>(n_samples, seed),  X.begin(), eig_static_to_dynamic); break;
        case 2:  rng::transform(gen_unit_dirs<6>(n_samples, seed),  X.begin(), eig_static_to_dynamic); break;
        case 3:  rng::transform(gen_unit_dirs<9>(n_samples, seed),  X.begin(), eig_static_to_dynamic); break;
        case 4:  rng::transform(gen_unit_dirs<12>(n_samples, seed), X.begin(), eig_static_to_dynamic); break;
        case 5:  rng::transform(gen_unit_dirs<15>(n_samples, seed), X.begin(), eig_static_to_dynamic); break;
        default: debug::check_expr(false, "Not implemented!");
      }
      return X;
    }
  } // namespace detail

  Basis::vec_type solve_spectrum_coef(const DirectSpectrumInfo &info) {
    met_trace();
    
    // Take a grayscale spectrum as mean to build around
    Spec mean = Spec(luminance(info.linear_constraints[0].second)).cwiseMin(1.f);
    
    // This object will store settings, constraints, objectives, and is then passed to solver
    opt::Wrapper<wavelength_bases> solver = {
      .x_init       = 0.05,
      .upper        = 1.0,
      .lower        =-1.0,
      .max_iters    = 256,
      .rel_xpar_tol = 1e-3, // Threshold for objective error
    };
    
    // Objective function minimizes l2 norm as a simple way to get relatively smooth coeffs
    solver.objective = opt::func_norm<wavelength_bases>(info.basis.func, mean);

    // Add color system equality constraints, upholding spectral metamerism
    for (const auto [csys, colr] : info.linear_constraints) {
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
    auto [coeffs, code] = solve(solver);
    return coeffs.cast<float>().eval();
  }

  Basis::vec_type solve_spectrum_coef(const IndirectSpectrumInfo &info) {
    met_trace();
    // Not implemented, not used; we really want to avoid solving this problem
    // and instead rely on the mmv boundary to interpolate an interior value
    return Basis::vec_type(0);
  }
  
  std::vector<Basis::vec_type> solve_mismatch_solid_coef(const DirectMismatchSolidInfo &info) {
    met_trace();


    // Sample unit vectors in nd
    auto samples_nd = detail::gen_unit_dirs(info.linear_objectives.size(), info.n_samples, info.seed);

    // Solver settings
    opt::Wrapper<wavelength_bases> solver = {
      .x_init       = 0.05,
      .upper        = 1.0,
      .lower        =-1.0,
      .max_iters    = 128,
      .rel_xpar_tol = 1e-3, // Threshold for objective error
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
    for (const auto [csys, colr] : info.linear_constraints) {
      auto A = (csys.finalize(false).transpose() * info.basis.func).eval();
      auto b = lrgb_to_xyz(colr);
      solver.eq_constraints.push_back({ .f   = opt::func_norm<wavelength_bases>(A, b), 
                                        .tol = 1e-3 });
    }

    // Construct objective matrix from color system; follow Mackiewicz et al. to acquire
    // more efficient matrix U instead (see listing 9)
    eig::MatrixXf S(wavelength_samples, 3 * info.linear_objectives.size());
    for (uint i = 0; i < info.linear_objectives.size(); ++i)
      S.block<wavelength_samples, 3>(0, 3 * i) = info.linear_objectives[i].finalize(false);
    
    // Construct orthonormal matrix U instead, following listing 9 of Mackiewicz et al.
    {
      eig::JacobiSVD<eig::MatrixXf> svd;
      svd.compute(S, eig::ComputeFullV);
      auto U = (S * svd.matrixV() * svd.singularValues().asDiagonal().inverse()).eval();
      S = U;
    }
    
    // Output data structure 
    std::vector<Basis::vec_type> output;
    output.reserve(info.n_samples);

    // Parallel solve for boundary spectra
    #pragma omp parallel
    {
      // Per thread copy of current solver parameter set
      auto local_solver = solver;

      // Iterate sampled unit vectors over threads
      #pragma omp for
      for (int i = 0; i < samples_nd.size(); ++i) {
        // Define objective function: max (Uk)^T (B x -> max (C^T B) x -> max dot(a, x)
        Basis::vec_type a = ((S * samples_nd[i]).transpose().eval() * info.basis.func).transpose().eval();
        local_solver.objective = opt::func_dot<wavelength_bases>(a, 0.f);
          
        // Run solver and store recovered spectral distribution if it is safe
        auto [coeffs, code] = solve(local_solver);
        guard_continue(!coeffs.array().isNaN().any() && !coeffs.array().isZero());
        #pragma omp critical
        {
          output.push_back(coeffs.cast<float>().eval());
        }
      } // for (int i)
    }

    return output;
  }

  std::vector<Basis::vec_type> solve_mismatch_solid_coef(const IndirectMismatchSolidInfo &info) {
    met_trace();

    // Helper value; basis functions in double
    auto B = info.basis.func.cast<double>().eval();                      

    // This object will store settings, constraints, objectives, and is then passed to solver
    opt::Wrapper<wavelength_bases> solver = {
      .x_init       = 0.05,
      .upper        = 1.0,
      .lower        =-1.0,
      .max_iters    = 256,
      .rel_xpar_tol = 1e-3, // Threshold for objective error
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
    for (const auto [csys, colr] : info.linear_constraints) {
      auto A = (csys.finalize(false).transpose() * info.basis.func).eval();
      auto b = lrgb_to_xyz(colr);
      solver.eq_constraints.push_back({ .f   = opt::func_norm<wavelength_bases>(A, b), 
                                        .tol = 1e-3 });
    }

    // Add indirect color system equality constraints, upholding uplifting roundtrip
    for (const auto [csys, colr] : info.nlinear_constraints) {
      auto A = csys.finalize(false)
             | vws::transform([](const CMFS &cmfs) { return cmfs.transpose().cast<double>().eval(); })
             | view_to<std::vector<eig::Matrix<double, 3, wavelength_samples>>>();
      auto b = lrgb_to_xyz(colr).cast<double>().eval();
      
      // Specify constraint
      solver.eq_constraints.push_back({ .f = ad::wrap_capture<wavelength_bases>([A, B, b](const bvec &x) {
        svec r  = B * x;                      // Compute full reflectance
        auto Ax = (A[0].rowwise().sum()       // Nonlinear component, 0th part
                +  A[1] * r).eval();          // Nonlinear component, 1st part
        for (uint i = 2; i < A.size(); ++i) { // Nonlinear component, pth part
          r = r.cwiseProduct(r);
          Ax += A[i] * r;
        }
        return (Ax.array() - b).matrix().norm();
      }), .tol = 1e-3 });
    }

    // Construct objective matrices from color system and power series
    std::vector<eig::MatrixXf> S;
    // Helper value; dynamic matrix of specific size set to all zeroes
    eig::MatrixXf S_zero(wavelength_samples, 3 * info.nlinear_objectives.size());
    S_zero.fill(0.f);
    for (uint i = 0; i < info.nlinear_objectives.size(); ++i) {
      auto powers = info.nlinear_objectives[i].finalize(false);
      if (S.size() < powers.size())
        S.resize(powers.size(), S_zero);
      for (uint j = 0; j < powers.size(); ++j)
        S[j].block<wavelength_samples, 3>(0, 3 * i) = powers[j];
    }

    // Sample unit vectors in 3*nd; total nr. of nonlinear objectives is counted
    auto samples = detail::gen_unit_dirs(info.nlinear_objectives.size(), info.n_samples, info.seed);

    // Output data structure 
    std::vector<Basis::vec_type> output;
    output.reserve(info.n_samples);

    // Parallel solve for boundary spectra
    #pragma omp parallel 
    {
      // Per thread copy of current solver parameter set
      auto local_solver = solver;

      // Iterate sampled unit vectors over threads
      #pragma omp for
      for (int i = 0; i < samples.size(); ++i) {
        // Project nonlinear objective matrices along sampled unit vector
        auto A = S | vws::transform([sample = samples[i]](const eig::MatrixXf &cmfs) {
          return eig::Vector<double, wavelength_samples>((cmfs * sample).cast<double>());
        }) | view_to<std::vector<eig::Vector<double, wavelength_samples>>>();

        // Specify objective
        local_solver.objective = ad::wrap_capture<wavelength_bases>([A, B](const bvec &x) {
          svec r = B * x;                       // Compute full reflectance
          auto f = A[0].sum()                   // Nonlinear objective, 0th part
                 + A[1].dot(r);                 // Nonlinear objective, 1st part
          for (uint i = 2; i < A.size(); ++i) { // Nonlinear objective, pth part
            r = r.cwiseProduct(r);
            f += A[i].dot(r);
          }
          return f;
        });

        // Run solver and store recovered spectral distribution if it is safe
        auto [coeffs, code] = solve(local_solver);
        guard_continue(!coeffs.array().isNaN().any() && !coeffs.array().isZero());

        #pragma omp critical
        {
          output.push_back(coeffs.cast<float>().eval());
        }
      } // for (int i)
    }
    
    return output;
  }

  std::vector<Basis::vec_type> solve_color_solid_coef(const ColorSolidInfo &info) {
    met_trace();

    // Sample unit vectors in 3d
    auto samples = detail::gen_unit_dirs<3>(info.n_samples, info.seed);

    // Output for parallel solve
    std::vector<Basis::vec_type> output;
    output.reserve(samples.size());

    // Construct objective function
    auto A = info.direct_objective.finalize();

    // Parallel solve for boundary spectra; iterate sample vectors
    #pragma omp parallel for
    for (int i = 0; i < samples.size(); ++i) {
      // Obtain actual spectrum by projecting sample onto optimal
      Spec s = (A * samples[i].matrix()).eval();
      for (auto &f : s)
        f = f >= 0.f ? 1.f : 0.f;

      // Find nearest valid spectrum within basis
      auto c = solve_spectrum_coef(DirectSpectrumInfo {
        .linear_constraints = {{ info.direct_objective, info.direct_objective(s) }},
        .basis = info.basis
      });

      // Store valid sample
      guard_continue(!c.array().isNaN().any() && !c.array().isZero());
      #pragma omp critical
      {
        output.push_back(c);
      }
    }
    
    return output;
  }

  std::vector<MismatchSample> solve_color_solid(const ColorSolidInfo &info) {
    met_trace();
    auto coeffs = solve_color_solid_coef(info);
    std::vector<MismatchSample> s(coeffs.size());
    std::transform(std::execution::par_unseq,
                   range_iter(coeffs),
                   s.begin(),
                   [&](const auto &coef) { 
                    auto spec = info.basis(coef);
                    auto colr = info.direct_objective(spec);
                    return MismatchSample { colr, spec, coef }; 
                  });
    return s;
  }

  Basis::vec_type solve_spectrum_coef(const SpectrumCoeffsInfo &info) {
    met_trace();
    
    // This object will store settings, constraints, objectives, and is then passed to solver
    opt::Wrapper<wavelength_bases> solver = {
      .x_init       = 0.05,
      .upper        = 1.0,
      .lower        =-1.0,
      .max_iters    = 512,   // Failsafe
      .rel_xpar_tol = 1e-5, // Threshold for objective error
    };

    // Add boundary inequality constraints, upholding spectral 0 <= x <= 1
    {
      auto A = (eig::Matrix<float, 2 * wavelength_samples, wavelength_bases>()
        << info.basis.func, -info.basis.func).finished();
      auto b = (eig::Array<float, 2 * wavelength_samples, 1>()
        << Spec(1.f), Spec(0.f)).finished();
      solver.nq_constraints_v.push_back({ .f   = opt::func_dot_v<wavelength_bases>(A, b),
                                          .n   = 2 * wavelength_samples, 
                                          .tol = 1e-3 });
    }

    // Objective function minimizes l2 norm over spectral distribution differences
    solver.objective = opt::func_squared_norm<wavelength_bases>(info.basis.func, info.spec);
    
    // Run solve and return result
    auto [coeffs, code] = solve(solver);
    return coeffs.cast<float>().cwiseMax(-1.f).cwiseMin(1.f).eval();
  }
  
  std::vector<MismatchSample> solve_mismatch_solid(const DirectMismatchSolidInfo &info) {
    met_trace();
    auto c = solve_mismatch_solid_coef(info);
    std::vector<MismatchSample> v(c.size());
    std::transform(std::execution::par_unseq,
                   range_iter(c), v.begin(),
                   [&info](const auto &c) { 
                    auto spec = info.basis(c);
                    auto colr = info.linear_objectives.back()(spec);
                    return MismatchSample { colr, spec, c }; }); // the differentiating color system generates output
    return v;
  }
  
  std::vector<MismatchSample> solve_mismatch_solid(const IndirectMismatchSolidInfo &info) {
    met_trace();
    auto c = solve_mismatch_solid_coef(info);
    std::vector<MismatchSample> v(c.size());
    std::transform(std::execution::par_unseq,
                   range_iter(c), v.begin(),
                   [&info](const auto &c) { 
                    auto spec = info.basis(c);
                    auto colr = info.nlinear_objectives.back()(spec);
                    return MismatchSample { colr, spec, c }; }); // the differentiating color system generates output
    return v;
  }
} // namespace met