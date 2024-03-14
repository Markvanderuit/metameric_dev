#include <metameric/core/nlopt.hpp>
#include <metameric/core/metamer.hpp>
#include <metameric/core/ranges.hpp>
#include <metameric/core/utility.hpp>
#include <oneapi/tbb/concurrent_vector.h>
#include <algorithm>
#include <execution>
#include <unordered_set>

namespace met {
  Spec generate_spectrum(GenerateSpectrumInfo info) {
    met_trace();
    debug::check_expr(info.systems.size() == info.signals.size(),
      "Color system size not equal to color signal size");

    // Take a grayscale spectrum as mean to build around
    Spec mean = info.signals[0].matrix().dot(eig::Vector3f { 0.2126, 0.7252, 0.0722 });

    // Solver settings
    NLOptInfoT<wavelength_bases> solver = {
      .algo         = NLOptAlgo::LD_SLSQP,
      .form         = NLOptForm::eMinimize,
      .x_init       = 0.5,
      .max_iters    = 512,  // Failsafe
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

  Spec generate_spectrum(GenerateIndirectSpectrumInfo info) {
    met_trace();

    // Solver settings
    NLOptInfoT<wavelength_bases> solver = {
      .algo         = NLOptAlgo::LD_SLSQP,
      .form         = NLOptForm::eMinimize,
      .x_init       = 0.5,
      .max_iters    = 512,  // Failsafe
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
  }

  std::vector<Spec> generate_mmv_boundary_spec(const GenerateMMVBoundaryInfo &info) {
    met_trace();

    // Solver settings
    NLOptInfoT<wavelength_bases> solver = {
      .algo         = NLOptAlgo::LD_SLSQP,
      .form         = NLOptForm::eMaximize,
      .x_init       = 0.5,
      .max_iters    = 512,  // Failsafe
      .rel_xpar_tol = 1e-2, // Threshold for objective error
    };

    // Construct orthogonal matrix used during maximiation
    auto S = rng::fold_left_first(info.systems_j, std::plus<CMFS> {}).value().eval();
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
    tbb_output.reserve(info.samples.size());
    #pragma omp parallel
    {
      // Per thread copy of current solver parameter set
      auto local_solver = solver;

      #pragma omp for
      for (int i = 0; i < info.samples.size(); ++i) {
        // Define objective function: max (Uk)^T (Bx) -> max C^T Bx -> max ax
        auto a = ((U * info.samples[i].matrix()).transpose() * info.basis.func).transpose().eval();
        local_solver.objective = detail::func_dot<wavelength_bases>(a, 0.f);

        // Run solver and store recovered spectral distribution if it is legit
        auto r = solve(local_solver);
        guard_continue(!r.x.array().isNaN().any());
        tbb_output.push_back((info.basis.func * r.x.cast<float>()).cwiseMax(0.f).cwiseMin(1.f).eval());
      } // for (int i)
    }

    return std::vector<Spec>(range_iter(tbb_output));
  }

  std::vector<Spec> generate_mmv_boundary_spec(const GenerateIndirectMMVBoundaryInfo &info) {
    met_trace();

    // Solver settings
    NLOptInfoT<wavelength_bases> solver = {
      .algo         = NLOptAlgo::LD_SLSQP,
      .form         = NLOptForm::eMinimize,
      .x_init       = 0.5,
      .max_iters    = 512,  // Failsafe
      .rel_xpar_tol = 1e-2, // Threshold for objective error
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
    tbb_output.reserve(info.samples.size());
    #pragma omp parallel
    {
      // Per thread copy of current solver parameter set
      auto local_solver = solver;

      #pragma omp for
      for (int i = 0; i < info.samples.size(); ++i) {
        using vec = NLOptInfoT<wavelength_bases>::vec;
        auto A = info.systems_j
               | vws::transform([s = info.samples[i]](const CMFS &cmfs) {
                 return (cmfs * s.matrix()).cast<double>().eval();
               }) | rng::to<std::vector>();

        local_solver.objective = 
          [A = A, B = info.basis.func.cast<double>().eval()]
          (eig::Map<const vec> x, eig::Map<vec> g) -> double {
            // Recover spectral distribution
            auto r = (B * x).eval();

            double diff =  A[0].sum();
            vec grad = 0.0;
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
  }

  std::vector<Spec> generate_ocs_boundary_spec(const GenerateOCSBoundaryInfo &info) {
    met_trace();

    // Run multithreaded spectrum generation
    std::vector<Spec> output(info.samples.size());
    std::transform(std::execution::par_unseq, range_iter(info.samples), output.begin(), [&](const Colr &sample) {
      // Obtain spectrum by projecting sample onto optimal
      Spec s = (info.system * sample.matrix()).eval();
      for (auto &f : s)
        f = f >= 0.f ? 1.f : 0.f;
      
      // Run solve to find nearest spectrum within the basis function set
      std::vector<CMFS> systems = { info.system };
      std::vector<Colr> signals = { (info.system.transpose() * s.matrix()).eval() };
      return generate_spectrum({  .basis = info.basis, .systems = systems, .signals = signals });
    });
    
    // Filter NaNs at underconstrained output and strip redundancy from return 
    std::erase_if(output, [](Spec &c) { return c.isNaN().any(); });
    std::unordered_set<
      Spec, 
      decltype(eig::detail::matrix_hash<float>), 
      decltype(eig::detail::matrix_equal)
    > output_unique(range_iter(output));
    return std::vector<Spec>(range_iter(output_unique));
  }
} // namespace met