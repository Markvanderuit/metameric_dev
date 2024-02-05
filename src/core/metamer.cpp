#include <metameric/core/nlopt.hpp>
#include <metameric/core/metamer.hpp>
#include <metameric/core/utility.hpp>
#include <algorithm>
#include <execution>
#include <unordered_set>

namespace met {
  constexpr uint min_wavelength_bases = 4;

  Spec generate_spectrum(GenerateSpectrumInfo info) {
    met_trace();
    debug::check_expr(info.systems.size() == info.signals.size(),
      "Color system size not equal to color signal size");

    NLOptInfo solver = {
      .n            = wavelength_bases,
      .algo         = NLOptAlgo::LD_SLSQP,
      .form         = NLOptForm::eMinimize,
      .x_init       = Basis::BVec(0.5).cast<double>().eval(),
      .max_iters    = 10,
      .rel_func_tol = 1e-3,
      .rel_xpar_tol = 1e-2,
    };
    

    // Objective function minimizes squared l2 norm
    solver.objective = [&](eig::Map<const eig::VectorXd> x, eig::Map<eig::VectorXd> g) {
      if (g.size())
        g = (2.0 * x).eval();
      return x.dot(x);
    };
    
    // Construct basis matrix and boundary vectors
    auto basis = info.basis.func.cast<double>().eval();
    Spec upper_bounds = Spec(1.0) - info.basis.mean;
    Spec lower_bounds = upper_bounds - Spec(1.0); 

    // Add boundary inequality constraints
    if (info.impose_boundedness) {
      for (uint i = 0; i < wavelength_samples; ++i) {
        solver.nq_constraints.push_back(
          [A = basis.row(i).eval(), b = upper_bounds[i]]
          (eig::Map<const eig::VectorXd> x, eig::Map<eig::VectorXd> g) {
            double f = A.dot(x);
            if (g.size())
              g = A;
            return f - b;
        });
        solver.nq_constraints.push_back(
          [A = basis.row(i).eval(), b = lower_bounds[i]]
          (eig::Map<const eig::VectorXd> x, eig::Map<eig::VectorXd> g) {
            double f = -A.dot(x);
            if (g.size())
              g = -A;
            return f + b;
        });
      }
    } // if (impose_boundedness)

    // Add color system equality constraints
    for (uint i = 0; i < info.systems.size(); ++i) {
      Colr o = (info.systems[i].transpose() * info.basis.mean.matrix()).transpose().eval();
      auto A = (info.systems[i].transpose().cast<double>() * basis).eval();
      auto b = (info.signals[i] - o).cast<double>().eval();

      for (uint j = 0; j < 3; ++j) {
        solver.eq_constraints.push_back(
          [A = A.row(j).eval(), b = b[j]]
          (eig::Map<const eig::VectorXd> x, eig::Map<eig::VectorXd> grad) {
            double f = A.dot(x);
            if (grad.size())
              grad = A;
            return f - b;
        });
      } // for (uint j)
    } // for (uint i)

    // Optimize result and return
    NLOptResult r = solve(solver);
    Spec s = info.basis.mean + Spec((basis * r.x).cast<float>());
    return s;

   /*  // Out-of-loop state
    bool is_first_run = true;
    Spec s = 0;

    const uint N = info.basis_count;
    const uint M = 3 * info.systems.size() 
                + (info.impose_boundedness ? 2 * wavelength_samples : 0);

    // Initialize parameter object for LP solver with expected matrix sizes M, N
    LPParameters params(M, N);

    // Obtain appropriate nr. of basis functions from data
    eig::MatrixXf basis = info.basis.func.block(0, 0, wavelength_samples, N).eval();

    // Construct basis bounds
    Spec upper_bounds = Spec(1.0) - info.basis.mean;
    Spec lower_bounds = upper_bounds - Spec(1.0); 

    // Add constraints to ensure resulting spectra produce the given color signals
    for (uint i = 0; i < info.systems.size(); ++i) {
      Colr signal_offs = (info.systems[i].transpose() * info.basis.mean.matrix()).transpose().eval();
      params.A.block(3 * i, 0, 3, N) = (info.systems[i].transpose() * basis).cast<double>().eval();
      params.b.block(3 * i, 0, 3, 1) = (info.signals[i] - signal_offs).cast<double>().eval();
    }

    // Add constraints to ensure resulting spectra are bounded to [0, 1]
    if (info.impose_boundedness) {
      const uint offs_l = 3 * info.systems.size();
      const uint offs_u = offs_l + wavelength_samples;
      params.A.block(offs_l, 0, wavelength_samples, N) = basis.cast<double>().eval();
      params.A.block(offs_u, 0, wavelength_samples, N) = basis.cast<double>().eval();
      params.b.block<wavelength_samples, 1>(offs_l, 0) = lower_bounds.cast<double>().eval();
      params.b.block<wavelength_samples, 1>(offs_u, 0) = upper_bounds.cast<double>().eval();
      params.r.block<wavelength_samples, 1>(offs_l, 0) = LPCompare::eGE;
      params.r.block<wavelength_samples, 1>(offs_u, 0) = LPCompare::eLE;
    }

    // Average min/max objectives for a nice smooth result
    params.objective = LPObjective::eMaximize;
    auto [opt_max, res_max] = lp_solve_res(params);
    params.objective = LPObjective::eMinimize;
    auto [opt_min, res_min] = lp_solve_res(params);

    // Obtain spectral reflectance
    Spec s_max = info.basis.mean + Spec(basis * res_max.cast<float>().matrix());
    Spec s_min = info.basis.mean + Spec(basis * res_min.cast<float>().matrix());
    return (0.5 * (s_min + s_max)).eval(); */
  }

  /* Spec generate_spectrum_recursive(GenerateSpectrumInfo info) {
    met_trace();
    debug::check_expr(info.systems.size() == info.signals.size(),
                          "Color system size not equal to color signal size");
           
    // Out-of-loop state
    bool is_first_run = true;
    Spec s = 0;

    while (info.basis_count > min_wavelength_bases) {
      const uint N = info.basis_count;
      const uint M = 3 * info.systems.size() + (info.impose_boundedness ? 2 * wavelength_samples : 0);

      // Initialize parameter object for LP solver with expected matrix sizes M, N
      LPParameters params(M, N);

      // Obtain appropriate nr. of basis functions from data
      eig::MatrixXf basis = info.basis.block(0, 0, wavelength_samples, N).eval();

      // Construct basis bounds
      Spec upper_bounds = Spec(1.0) - info.basis.mean;
      Spec lower_bounds = upper_bounds - Spec(1.0); 

      // Normalized sensitivity weight minimization to prevent border issues
      Spec w = (info.systems[0].rowwise().sum() / 3.f).eval();
      w = ((w / w.sum())).eval();
      params.C = (w.matrix().transpose() * basis).transpose().cast<double>().eval();   

      // Add constraints to ensure resulting spectra produce the given color signals
      for (uint i = 0; i < info.systems.size(); ++i) {
        Colr signal_offs = (info.systems[i].transpose() * info.basis.mean.matrix()).transpose().eval();
        params.A.block(3 * i, 0, 3, N) = (info.systems[i].transpose() * basis).cast<double>().eval();
        params.b.block(3 * i, 0, 3, 1) = (info.signals[i] - signal_offs).cast<double>().eval();
      }

      // Add constraints to ensure resulting spectra are bounded to [0, 1]
      if (info.impose_boundedness) {
        const uint offs_l = 3 * info.systems.size();
        const uint offs_u = offs_l + wavelength_samples;
        params.A.block(offs_l, 0, wavelength_samples, N) = basis.cast<double>().eval();
        params.A.block(offs_u, 0, wavelength_samples, N) = basis.cast<double>().eval();
        params.b.block<wavelength_samples, 1>(offs_l, 0) = lower_bounds.cast<double>().eval();
        params.b.block<wavelength_samples, 1>(offs_u, 0) = upper_bounds.cast<double>().eval();
        params.r.block<wavelength_samples, 1>(offs_l, 0) = LPCompare::eGE;
        params.r.block<wavelength_samples, 1>(offs_u, 0) = LPCompare::eLE;
      }

      // Average min/max objectives for a nice smooth result
      params.objective = LPObjective::eMaximize;
      auto [opt_max, res_max] = lp_solve_res(params);
      params.objective = LPObjective::eMinimize;
      auto [opt_min, res_min] = lp_solve_res(params);

      // Obtain spectral reflectance
      Spec s_max = info.basis.mean + Spec(basis * res_max.cast<float>().matrix());
      Spec s_min = info.basis.mean + Spec(basis * res_min.cast<float>().matrix());
      Spec s_new = (0.5 * (s_min + s_max)).eval();

      // On first run, obtain any (possibly infeasible) result
      if (is_first_run)
        s = s_new;

      // On secondary runs, continue only if the system remains feasible
      guard_break(info.reduce_basis_count && opt_min && opt_max);
      info.basis_count--;
      s = s_new;
    }               
    
    return s;
  } */

  std::vector<Spec> generate_ocs_boundary_spec(const GenerateOCSBoundaryInfo &info) {
    met_trace();

    std::vector<Spec> output(info.samples.size());

    std::transform(std::execution::par_unseq, range_iter(info.samples), output.begin(), [&](const Colr &sample) {
      Spec s = (info.system * sample.matrix()).eval();
      for (auto &f : s)
        f = f >= 0.f ? 1.f : 0.f;

      // Find nearest generalized spectrum that fits within the basis function approach
      return generate_spectrum({
        .basis      = info.basis,
        .systems    = std::vector<CMFS> { info.system },
        .signals    = std::vector<Colr> { (info.system.transpose() * s.matrix()).eval() }
      });
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

  std::vector<Colr> generate_ocs_boundary_colr(const GenerateOCSBoundaryInfo &info) {
    met_trace();

    std::vector<Spec> input = generate_ocs_boundary_spec(info);
    std::vector<Colr> output(input.size());

    std::transform(std::execution::par_unseq, range_iter(input), output.begin(), [&](const Spec &s) {
      return (info.system.transpose() * s.matrix()).eval();
    });
    
    // Filter NaNs at underconstrained output and strip redundancy from return 
    std::erase_if(output, [](Colr &c) { return c.isNaN().any(); });
    std::unordered_set<
      Colr, 
      decltype(Eigen::detail::matrix_hash<float>), 
      decltype(Eigen::detail::matrix_equal)
    > output_unique(range_iter(output));
    return std::vector<Colr>(range_iter(output_unique));
  }

  std::vector<Spec> generate_mmv_boundary_spec(const GenerateMMVBoundaryInfo &info) {
    met_trace();

    // Construct basis matrix
    auto B = info.basis.func.cast<double>().eval();
    auto basis_trf = 
      [B](const auto &csys) { return (csys.transpose().cast<double>() * B).eval(); };

    NLOptInfo solver = {
      .n            = wavelength_bases,
      .algo         = NLOptAlgo::LD_SLSQP,
      .form         = NLOptForm::eMaximize,
      .x_init       = eig::Vector<double, wavelength_bases>(0.5),
      // .max_iters    = 100,
      .rel_func_tol = 1e-2,
      .rel_xpar_tol = 1e-3,
    };

    // Construct orthogonal matrix used during maximiation
    auto S = rng::fold_left_first(info.systems_j, std::plus<CMFS> {}).value().cast<double>().eval();
    eig::JacobiSVD<decltype(S)> svd;
    svd.compute(S, eig::ComputeFullV);
    auto U = (S * svd.matrixV() * svd.singularValues().asDiagonal().inverse()).eval();

    // Add color system equality constraints
    for (uint i = 0; i < info.systems_i.size(); ++i) {
      auto A = basis_trf(info.systems_i[i]);
      auto b = info.signals_i[i].cast<double>().eval();
      for (uint j = 0; j < 3; ++j) {
        solver.eq_constraints.push_back(
          [A = A.row(j).eval(), b = b[j]]
          (eig::Map<const eig::VectorXd> x, eig::Map<eig::VectorXd> g) {
            using Spec = eig::Vector<double, wavelength_samples>;

            // Linear function:
            // - f(x) = A^T Bx - b == 0
            // - d(f) = A^T B
            if (g.size())
              g = A;
            return A.dot(x) - b;
        });
      } // for (uint j)
    } // for (uint i)

    // Add [0, 1] boundary inequality constraints
    for (uint i = 0; i < wavelength_samples; ++i) {
      solver.nq_constraints.push_back(
        [B = B.row(i).eval()]
        (eig::Map<const eig::VectorXd> x, eig::Map<eig::VectorXd> g) {
          if (g.size())
            g = B;
          return B.dot(x) - 1.0;
      });
      solver.nq_constraints.push_back(
        [B = B.row(i).eval()]
        (eig::Map<const eig::VectorXd> x, eig::Map<eig::VectorXd> g) {
          if (g.size())
            g = -B;
          return (-B).dot(x);
      });
    }

    // Output data vector
    std::vector<Spec> output(info.samples.size());

    // Parallel solve for basis function weights defining OCS boundary spectra
    #pragma omp parallel
    {
      // Per thread copy of current solver parameter set
      NLOptInfo local_solver = solver;

      #pragma omp for
      for (int i = 0; i < info.samples.size(); ++i) {
        // Define objective function: max (Uk)^T (Bx)^p -> max C^T (Bx)^p
        auto C = (U * info.samples[i].matrix().cast<double>()).eval();

        local_solver.objective = 
          [&C, &B/* , p = power, run_full_power = switch_power */]
          (eig::Map<const eig::VectorXd> x, eig::Map<eig::VectorXd> g) -> double {
            using Spec = eig::Vector<double, wavelength_samples>;
            
            // Power function:
            // - f(x) = C^T (Bx)^p
            // - d(f) = p * (C x (Bx)^{p - 1})^T * B
            // Spec X = B * x;
            // Spec px, dx;
            // if (run_full_power) {
            //   px = X.array().pow(p).eval();
            //   dx = X.array().pow(p - 1.0).eval();
            // } else {
            //   // px = X.array().pow(/* std::min(p, 4.0) */8.0).eval();
            //   // dx = X.array().pow(/* std::min(p, 4.0) */8.0 - 1.0).eval();
            //   px = p > 1.f ? X.cwiseProduct(X) : X;
            //   dx = p > 1.f ? X : 1.f;
            // }
            
            // if (g.size())
            //   g = p * (C.cwiseProduct(dx).transpose() * B).array();
            // return C.dot(px);

            // Linear function:
            // - f(x) = C^T Bx
            // - d(f) = C^T B
            Spec px = B * x;
            if (g.size())
              g = (C.transpose() * B).transpose().eval();
            return C.dot(px);
        };

        // Obtain result from solver
        NLOptResult r = solve(local_solver);

        // Return spectral result, raised to requested power
        Spec s = (B * r.x).cast<float>();
        
        output[i] = s/* .pow(power) */.cwiseMin(1.f).cwiseMax(0.f);
      } // for (int i)
    }

    // Filter NaNs at underconstrained output and strip redundancy from return 
    std::erase_if(output, [](Spec &c) { return c.isNaN().any(); });
    /* std::unordered_set<
      Spec, 
      decltype(Eigen::detail::matrix_hash<float>), 
      decltype(Eigen::detail::matrix_equal)
    > output_unique(range_iter(output)); */
    return output; // std::vector<Spec>(range_iter(output_unique));



    /* // Generate color system spectra for basis function parameters
    const uint N = wavelength_samples; // wavelength_bases;
    // auto basis = info.basis.func;
    auto basis = eig::Matrix<float, wavelength_samples, wavelength_samples>::Identity().eval();
    auto basis_trf = 
      [b = basis](const auto &csys) { return (csys.transpose() * b).eval(); };
    auto csys_j = basis_trf(info.system_j);
    auto csys_i = vws::transform(info.systems_i, basis_trf) | rng::to<std::vector>();
      
    // Initialize parameter object for LP solver, given expected matrix sizes
    const uint M = 3 * csys_i.size() + 2 * wavelength_samples;
    LPParameters params(M, N);
    params.objective = LPObjective::eMaximize;
    params.method    = LPMethod::ePrimal;

    // Add color system constraints
    for (uint i = 0; i < csys_i.size(); ++i) {
      params.A.block<3, N>(3 * i, 0) = csys_i[i].cast<double>();
      params.b.block<3, 1>(3 * i, 0) = info.signals_i[i].cast<double>();
    }

    // Add [0, 1] bounds constraints
    params.A.block<wavelength_samples, N>(csys_i.size() * 3, 0)                      = basis.cast<double>();
    params.A.block<wavelength_samples, N>(csys_i.size() * 3 + wavelength_samples, 0) = basis.cast<double>();
    params.b.block<wavelength_samples, 1>(csys_i.size() * 3, 0)                      = Spec(0.0).cast<double>();
    params.b.block<wavelength_samples, 1>(csys_i.size() * 3 + wavelength_samples, 0) = Spec(1.0).cast<double>();
    params.r.block<wavelength_samples, 1>(csys_i.size() * 3, 0)                      = LPCompare::eGE;
    params.r.block<wavelength_samples, 1>(csys_i.size() * 3 + wavelength_samples, 0) = LPCompare::eLE;

    // Obtain orthogonal basis functions through SVD of color system matrix
    eig::MatrixXf S(N, 3 + 3 * csys_i.size());
    for (uint i = 0; i < csys_i.size(); ++i)
      S.block<N, 3>(0, 3 * i) = csys_i[i].transpose();
    S.block<N, 3>(0, 3 * csys_i.size()) = csys_j.transpose();
    eig::JacobiSVD<decltype(S)> svd;
    svd.compute(S, eig::ComputeFullV);
    auto U = (S * svd.matrixV() * svd.singularValues().asDiagonal().inverse()).eval();

    // Parallel solve for basis function weights defining OCS boundary spectra
    std::vector<Spec> output(info.samples.size());
    #pragma omp parallel
    {
      LPParameters local_params = params;
      #pragma omp for
      for (int i = 0; i < info.samples.size(); ++i) {
        local_params.C = (U * info.samples[i].matrix()).cast<double>().eval();
        eig::Matrix<float, N, 1> w = lp_solve(local_params).cast<float>().eval();
        output[i] = basis * w;
      }
    }

    // Filter NaNs at underconstrained output and strip redundancy from return 
    std::erase_if(output, [](Spec &c) { return c.isNaN().any(); });
    std::unordered_set<
      Spec, 
      decltype(Eigen::detail::matrix_hash<float>), 
      decltype(Eigen::detail::matrix_equal)
    > output_unique(range_iter(output));
    return std::vector<Spec>(range_iter(output_unique)); */
  }

  std::vector<Colr> generate_mmv_boundary_colr(const GenerateMMVBoundaryInfo &info) {
    met_trace();

    // Generate unique boundary spectra
    auto spectra = generate_mmv_boundary_spec(info);

    // Transform to non-unique colors
    std::vector<Colr> colors(spectra.size());
    std::transform(std::execution::par_unseq, range_iter(spectra), colors.begin(),
      [&](const auto &s) -> Colr {  return (info.system_j.transpose() * s.matrix()).eval(); });

    // Collapse return value to unique colors
    std::unordered_set<
      Colr, 
      decltype(Eigen::detail::matrix_hash<float>), 
      decltype(Eigen::detail::matrix_equal)
    > output_unique(range_iter(colors));
    return std::vector<Colr>(range_iter(output_unique));

    /* // Generate color system spectra for basis function parameters
    const uint N = wavelength_bases;
    auto basis = info.basis.func;
    auto basis_trf = 
      [b = basis](const auto &csys) { return (csys.transpose() * b).eval(); };
    auto csys_j = basis_trf(info.system_j);
    auto csys_i = vws::transform(info.systems_i, basis_trf) | rng::to<std::vector>();

    // TODO remove this absolute hack
    auto csys_j_override = info.system_j_override.transform(basis_trf).value_or(csys_j);

    // Initialize parameter object for LP solver, given expected matrix sizes
    const uint M = 3 * csys_i.size() + 2 * wavelength_samples;
    LPParameters params(M, N);
    params.objective = LPObjective::eMaximize;
    params.method    = LPMethod::ePrimal;

    // Add color system constraints
    for (uint i = 0; i < csys_i.size(); ++i) {
      params.A.block<3, N>(3 * i, 0) = csys_i[i].cast<double>();
      params.b.block<3, 1>(3 * i, 0) = info.signals_i[i].cast<double>();
    }

    // Add [0, 1] bounds constraints
    params.A.block<wavelength_samples, N>(csys_i.size() * 3, 0)                      = basis.cast<double>();
    params.A.block<wavelength_samples, N>(csys_i.size() * 3 + wavelength_samples, 0) = basis.cast<double>();
    params.b.block<wavelength_samples, 1>(csys_i.size() * 3, 0)                      = Spec(0.0).cast<double>();
    params.b.block<wavelength_samples, 1>(csys_i.size() * 3 + wavelength_samples, 0) = Spec(1.0).cast<double>();
    params.r.block<wavelength_samples, 1>(csys_i.size() * 3, 0)                      = LPCompare::eGE;
    params.r.block<wavelength_samples, 1>(csys_i.size() * 3 + wavelength_samples, 0) = LPCompare::eLE;

    // Obtain orthogonal basis functions through SVD of color system matrix
    eig::MatrixXf S(N, 3 + 3 * csys_i.size());
    for (uint i = 0; i < csys_i.size(); ++i)
      S.block<N, 3>(0, 3 * i) = csys_i[i].transpose();
    S.block<N, 3>(0, 3 * csys_i.size()) = csys_j.transpose();
    eig::JacobiSVD<decltype(S)> svd;
    svd.compute(S, eig::ComputeFullV);
    auto U = (S * svd.matrixV() * svd.singularValues().asDiagonal().inverse()).eval();

    // Parallel solve for basis function weights defining OCS boundary spectra
    std::vector<Colr> output(info.samples.size());
    #pragma omp parallel
    {
      LPParameters local_params = params;
      #pragma omp for
      for (int i = 0; i < info.samples.size(); ++i) {
        local_params.C = (U * info.samples[i].matrix()).cast<double>().eval();
        eig::Matrix<float, N, 1> w = lp_solve(local_params).cast<float>().eval();
        output[i] = csys_j_override * w;
      }
    }

    // Filter NaNs at underconstrained output and strip redundancy from return 
    std::erase_if(output, [](Colr &c) { return c.isNaN().any(); });
    std::unordered_set<
      Colr, 
      decltype(Eigen::detail::matrix_hash<float>), 
      decltype(Eigen::detail::matrix_equal)
    > output_unique(range_iter(output));
    return std::vector<Colr>(range_iter(output_unique)); */
  }

  // row/col expansion shorthand for a given eigen matrix
  #define rowcol(mat) decltype(mat)::RowsAtCompileTime, decltype(mat)::ColsAtCompileTime

 /*  std::vector<Spec> generate_gamut(const GenerateGamutInfo &info) {
    // Constant and type shorthands
    using Signal = GenerateGamutInfo::Signal;
    constexpr uint n_bary = generalized_weights;
    constexpr uint n_spec = wavelength_samples;
    constexpr uint n_base = wavelength_bases;
    constexpr uint n_colr = 3;

    // Common matrix sizes
    constexpr uint N = n_bary * n_base;
    const     uint M = info.signals.size() * n_colr // Roundtrip constraints for seed samples
                     + info.gamut.size() * n_colr   // Roundtrip constraints for vertex positions
                     + 2 * n_bary * n_spec;         // Boundedness contraints for vertex spectra
    
    // Initialize parameter object for LP solver with expected matrix sizes
    LPParameters params(M, N);
    params.method    = LPMethod::ePrimal;
    params.scaling   = true;
    params.objective = LPObjective::eMinimize;
    
    // Construct basis bounds
    Spec upper_bounds = Spec(1.0) - info.basis.mean;
    Spec lower_bounds = upper_bounds - Spec(1.0); 

    // Clear untouched matrix values to 0
    params.A.fill(0.0);

    // Normalized sensitivity weight minimization to prevent border issues
    Spec w = (info.systems[0].rowwise().sum() / 3.f).eval(); // Average of three rows, not luminance?!
    w = (Spec(1.0) - (w / w.sum())).eval();
    auto C = (w.matrix().transpose() * info.basis.func).transpose().eval();

    // Specify objective function using the above weight
    for (uint i = 0; i <  n_bary; ++i)
      params.C.block(i * n_base, 0, rowcol(C)) = C.cast<double>().eval();

    // Add roundtrip constraints for seed samples
    for (uint i = 0; i < info.signals.size(); ++i) {
      const Signal &signal = info.signals[i];

      auto signal_csys = (info.systems[signal.syst_i].transpose() * info.basis.func).eval();
      Colr signal_avg  = (info.systems[signal.syst_i].transpose() * info.basis.mean.matrix()).transpose().eval();

      for (uint j = 0; j < n_bary; ++j) {
        auto A = (signal_csys * signal.bary_v[j]).cast<double>().eval();
        params.A.block(i * n_colr, j * n_base, rowcol(A)) = A;
      }
      
      auto b = (signal.colr_v - signal_avg).cast<double>().eval();
      params.b.block(i * n_colr, 0, rowcol(b)) = b;
    }

    // Add roundtrip constraints for gamut vertex positions
    const auto gamut_csys = (info.systems[0].transpose() * info.basis.func).cast<double>().eval();
    const uint gamut_offs = info.signals.size() * n_colr;
    const Colr gamut_avg  = (info.systems[0].transpose() * info.basis.mean.matrix()).transpose().eval();
    for (uint i = 0; i < info.gamut.size(); ++i) {
      auto b =( info.gamut[i] - gamut_avg).cast<double>().eval();
      params.A.block(gamut_offs + i * n_colr, i * n_base, rowcol(gamut_csys)) = gamut_csys;
      params.b.block(gamut_offs + i * n_colr, 0, rowcol(b)) = b;
    }

    // Add boundedness constraints for resulting spectra
    const uint l_offs = gamut_offs + info.gamut.size() * n_colr;
    const uint u_offs = l_offs + n_bary * n_spec;
    const auto basis = info.basis.func.cast<double>().eval();
    for (uint i = 0; i < n_bary; ++i) {
      params.A.block(l_offs + i * n_spec, i * n_base, rowcol(basis)) = basis;
      params.A.block(u_offs + i * n_spec, i * n_base, rowcol(basis)) = basis;
      params.b.block<wavelength_samples, 1>(l_offs + i * n_spec, 0) = lower_bounds.cast<double>().eval();
      params.b.block<wavelength_samples, 1>(u_offs + i * n_spec, 0) = upper_bounds.cast<double>().eval();
      params.r.block<wavelength_samples, 1>(l_offs + i * n_spec, 0) = LPCompare::eGE;
      params.r.block<wavelength_samples, 1>(u_offs + i * n_spec, 0) = LPCompare::eLE;
    }

    // Run solver and pray; cast results back to float
    params.objective = LPObjective::eMinimize;
    auto x_min = lp_solve(params).cast<float>().eval();
    params.objective = LPObjective::eMaximize;
    auto x_max = lp_solve(params).cast<float>().eval();

    // Obtain basis function weights from solution and compute resulting spectra
    std::vector<Spec> out(n_bary);
    for (uint i = 0; i < n_bary; ++i)
      out[i] = (info.basis.mean 
        + (info.basis.func 
          * eig::Matrix<float, wavelength_bases, 1>(x_min.block<n_base, 1>(n_base * i, 0))
          ).array().eval()
      ).cwiseMax(0.f).cwiseMin(1.f).eval();
    return out;
  } */
} // namespace met