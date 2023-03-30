#include <metameric/core/linprog.hpp>
#include <metameric/core/metamer.hpp>
#include <metameric/core/utility.hpp>
#include <metameric/core/detail/trace.hpp>
#include <omp.h>
#include <algorithm>
#include <execution>
#include <unordered_set>

namespace met {
  constexpr uint min_wavelength_bases = 4;

  Spec generate_spectrum(GenerateSpectrumInfo info) {
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
      Spec upper_bounds = Spec(1.0) - info.basis_mean;
      Spec lower_bounds = upper_bounds - Spec(1.0); 

      // Normalized sensitivity weight minimization to prevent border issues
      Spec w = (info.systems[0].rowwise().sum() / 3.f).eval();
      w = ((w / w.sum())).eval();
      params.C = (w.matrix().transpose() * basis).transpose().cast<double>().eval();   

      // Add constraints to ensure resulting spectra produce the given color signals
      for (uint i = 0; i < info.systems.size(); ++i) {
        Colr signal_offs = (info.systems[i].transpose() * info.basis_mean.matrix()).transpose().eval();
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
      Spec s_max = info.basis_mean + Spec(basis * res_max.cast<float>().matrix());
      Spec s_min = info.basis_mean + Spec(basis * res_min.cast<float>().matrix());
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
  }

  std::vector<Colr> generate_ocs_boundary(const GenerateOCSBoundaryInfo &info) {
    met_trace();

    std::vector<Colr> out(info.samples.size());

    std::transform(std::execution::par_unseq, range_iter(info.samples), out.begin(), [&](const Colr &sample) {
      Spec s = (info.system * sample.matrix()).eval();
      for (auto &f : s)
        f = f >= 0.f ? 1.f : 0.f;

      // Find nearest generalized spectrum that fits within the basis function approach
      Spec s_ = generate_spectrum({
        .basis      = info.basis,
        .basis_mean = info.basis_mean,
        .systems    = std::vector<CMFS> { info.system },
        .signals    = std::vector<Colr> { (info.system.transpose() * s.matrix()).eval() }
      });

      return (info.system.transpose() * s_.matrix()).eval();
    });

    return out;
  }
  
  std::vector<Colr> generate_mismatch_boundary(const GenerateMismatchBoundaryInfo &info) {
    met_trace();

    using Syst = eig::Matrix<float, 3, wavelength_bases>;
    
    // Generate color system spectra for basis function parameters
    auto csys_j = (info.system_j.transpose() * info.basis).eval();
    auto csys_i = std::vector<Syst>(info.systems_i.size());
    std::ranges::transform(info.systems_i, csys_i.begin(),
      [&](const auto &m) { return (m.transpose() * info.basis).eval(); });    

    // Initialize parameter object for LP solver, given expected matrix sizes
    const uint N = wavelength_bases;
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
    params.A.block<wavelength_samples, N>(csys_i.size() * 3, 0)                      = info.basis.cast<double>();
    params.A.block<wavelength_samples, N>(csys_i.size() * 3 + wavelength_samples, 0) = info.basis.cast<double>();
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
        eig::Matrix<float, wavelength_bases, 1> w = lp_solve(local_params).cast<float>().eval();
        output[i] = csys_j * w;
      }
    }

    // Filter NaNs at underconstrained output and strip redundancy from return 
    std::erase_if(output, [](Colr &c) { return c.isNaN().any(); });
    std::unordered_set<
      Colr, 
      decltype(Eigen::detail::matrix_hash<float>), 
      decltype(Eigen::detail::matrix_equal)
    > output_unique(range_iter(output));
    return std::vector<Colr>(range_iter(output_unique));
  }

  // row/col expansion shorthand for a given eigen matrix
  #define rowcol(mat) decltype(mat)::RowsAtCompileTime, decltype(mat)::ColsAtCompileTime

  std::vector<Spec> generate_gamut(const GenerateGamutInfo &info) {
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
    Spec upper_bounds = Spec(1.0) - info.basis_mean;
    Spec lower_bounds = upper_bounds - Spec(1.0); 

    // Clear untouched matrix values to 0
    params.A.fill(0.0);

    // Normalized sensitivity weight minimization to prevent border issues
    Spec w = (info.systems[0].rowwise().sum() / 3.f).eval(); // Average of three rows, not luminance?!
    w = (Spec(1.0) - (w / w.sum())).eval();
    auto C = (w.matrix().transpose() * info.basis).transpose().eval();

    // Specify objective function using the above weight
    for (uint i = 0; i <  n_bary; ++i)
      params.C.block(i * n_base, 0, rowcol(C)) = C.cast<double>().eval();

    // Add roundtrip constraints for seed samples
    for (uint i = 0; i < info.signals.size(); ++i) {
      const Signal &signal = info.signals[i];

      auto signal_csys = (info.systems[signal.syst_i].transpose() * info.basis).eval();
      Colr signal_avg  = (info.systems[signal.syst_i].transpose() * info.basis_mean.matrix()).transpose().eval();

      for (uint j = 0; j < n_bary; ++j) {
        auto A = (signal_csys * signal.bary_v[j]).cast<double>().eval();
        params.A.block(i * n_colr, j * n_base, rowcol(A)) = A;
      }
      
      auto b = (signal.colr_v - signal_avg).cast<double>().eval();
      params.b.block(i * n_colr, 0, rowcol(b)) = b;
    }

    // Add roundtrip constraints for gamut vertex positions
    const auto gamut_csys = (info.systems[0].transpose() * info.basis).cast<double>().eval();
    const uint gamut_offs = info.signals.size() * n_colr;
    const Colr gamut_avg  = (info.systems[0].transpose() * info.basis_mean.matrix()).transpose().eval();
    for (uint i = 0; i < info.gamut.size(); ++i) {
      auto b =( info.gamut[i] - gamut_avg).cast<double>().eval();
      params.A.block(gamut_offs + i * n_colr, i * n_base, rowcol(gamut_csys)) = gamut_csys;
      params.b.block(gamut_offs + i * n_colr, 0, rowcol(b)) = b;
    }

    // Add boundedness constraints for resulting spectra
    const uint l_offs = gamut_offs + info.gamut.size() * n_colr;
    const uint u_offs = l_offs + n_bary * n_spec;
    const auto basis = info.basis.cast<double>().eval();
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
      out[i] = (info.basis_mean 
        + (info.basis 
          * eig::Matrix<float, wavelength_bases, 1>(x_min.block<n_base, 1>(n_base * i, 0))
          ).array().eval()
      ).cwiseMax(0.f).cwiseMin(1.f).eval();
    return out;
  }
} // namespace met