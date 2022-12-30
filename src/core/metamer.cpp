#include <metameric/core/linprog.hpp>
#include <metameric/core/metamer.hpp>
#include <metameric/core/utility.hpp>
#include <metameric/core/detail/trace.hpp>
#include <omp.h>
#include <algorithm>
#include <execution>
#include <unordered_set>
#include <fmt/ranges.h>

namespace met {
  namespace detail {
    // key_hash for eigen types for std::unordered_map/unordered_set
    template <typename T>
    constexpr
    auto eig_hash = [](const auto &mat) {
      size_t seed = 0;
      for (size_t i = 0; i < mat.size(); ++i) {
        auto elem = *(mat.data() + i);
        seed ^= std::hash<T>()(elem) + 0x9e3779b9 + (seed << 6) + (seed >> 2);
      }
      return seed;
    };

    // key_equal for eigen types for std::unordered_map/unordered_set
    constexpr 
    auto eig_equal = [](const auto &a, const auto &b) { 
      return a.isApprox(b); 
    };
    
    template <typename T>
    using eig_hash_t  = decltype(eig_hash<T>);
    using eig_equal_t = decltype(eig_equal);

    std::vector<Colr> remove_identical_points(const std::vector<Colr> &v) {
      met_trace();
      std::unordered_set<Colr, eig_hash_t<float>, eig_equal_t> s(range_iter(v), 16);
      return std::vector<Colr>(range_iter(s));
    }
  } // namespace detail

  Spec generate(const BBasis         &basis,
                std::span<const CMFS> systems,
                std::span<const Colr> signals) {
    met_trace();
    debug::check_expr_dbg(systems.size() == signals.size(),
                          "Color system size not equal to color signal size");

    // Initialize parameter object for LP solver with expected matrix sizes M, N
    constexpr uint N = wavelength_bases;
    const     uint M = 3 * systems.size() + 2 * wavelength_samples;
    LPParameters params(M, N);
    params.method  = LPMethod::ePrimal;
    params.scaling = true;

    // Add constraints to ensure resulting spectra produce the given color signals
    for (uint i = 0; i < systems.size(); ++i) {
      params.A.block<3, N>(3 * i, 0) = (systems[i].transpose() * basis).cast<double>().eval();
      params.b.block<3, 1>(3 * i, 0) = signals[i].cast<double>().eval();
    }

    // Add constraints to restrict resulting spectra are bounded to [0, 1]
    const uint offs_l = 3 * systems.size();
    const uint offs_u = offs_l + wavelength_samples;
    params.A.block<wavelength_samples, N>(offs_l, 0) = basis.cast<double>().eval();
    params.A.block<wavelength_samples, N>(offs_u, 0) = basis.cast<double>().eval();
    params.b.block<wavelength_samples, 1>(offs_l, 0) = 0.0;
    params.b.block<wavelength_samples, 1>(offs_u, 0) = 1.0;
    params.r.block<wavelength_samples, 1>(offs_l, 0) = LPCompare::eGE;
    params.r.block<wavelength_samples, 1>(offs_u, 0) = LPCompare::eLE;

    // Solve for minimized/maximized results and take average
    params.objective = LPObjective::eMinimize;
    BSpec minv = lp_solve(params).cast<float>();
    params.objective = LPObjective::eMaximize;
    BSpec maxv = lp_solve(params).cast<float>();
    return basis * 0.5f * (minv + maxv);
  }
  
  std::vector<Colr> generate_boundary_i(const BBasis &basis,
                                        std::span<const CMFS> systems_i,
                                        std::span<const Colr> signals_i,
                                        const CMFS &system_j,
                                        std::span<const eig::ArrayXf> samples) {
    met_trace();

    using Syst = eig::Matrix<float, 3, wavelength_bases>;
    
    // Generate color system spectra for basis function parameters
    auto csys_j = (system_j.transpose() * basis).eval();
    auto csys_i = std::vector<Syst>(systems_i.size());
    std::ranges::transform(systems_i, csys_i.begin(),
      [&](const auto &m) { return (m.transpose() * basis).eval(); });    

    // Initialize parameter object for LP solver, given expected matrix sizes
    constexpr uint N = wavelength_bases;
    const     uint M = 3 * csys_i.size() + 2 * wavelength_samples;
    LPParameters params(M, N);
    params.objective = LPObjective::eMinimize;
    params.method    = LPMethod::eDual;
    params.scaling   = true;

    // Add color system constraints
    for (uint i = 0; i < csys_i.size(); ++i) {
      params.A.block<3, N>(3 * i, 0) = csys_i[i].cast<double>();
      params.b.block<3, 1>(3 * i, 0) = signals_i[i].cast<double>();
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

    // Define return object
    std::vector<Colr> output(samples.size());

    // Parallel solve for basis function weights defining OCS boundary spectra
    #pragma omp parallel
    {
      LPParameters local_params = params;
      #pragma omp for
      for (int i = 0; i < samples.size(); ++i) {
        local_params.C = (U * samples[i].matrix()).cast<double>().eval();
        BSpec w = lp_solve(local_params).cast<float>().eval();
        output[i] = csys_j * w;
      }
    }

    return detail::remove_identical_points(output);
  }

  // row/col expansion shorthand for a given eigen matrix
  #define rowcol(mat) decltype(mat)::RowsAtCompileTime, decltype(mat)::ColsAtCompileTime
  
  std::vector<Spec> generate_gamut(const GenerateSpectralGamutInfo &info) {
    // Constant shorthands
    constexpr uint n_bary = barycentric_weights;
    constexpr uint n_spec = wavelength_samples;
    constexpr uint n_base = wavelength_bases;
    constexpr uint n_colr = 3;
    constexpr double gamut_err = 0.0; //5;
    
    // Common matrix sizes
    constexpr uint N = n_bary * n_base;
    const     uint M = info.samples.size() * n_spec
                     + 1 * info.gamut.size() * n_colr
                     + 2 * n_bary * n_spec;
    
    // Initialize parameter object for LP solver with expected matrix sizes
    LPParameters params(M, N);
    params.method    = LPMethod::ePrimal;
    params.scaling   = true;
    params.objective = LPObjective::eMinimize;

    // Clear untouched matrix values to 0
    params.A.fill(0.0);
    
    // Add constraints to ensure weighted combinations produce the given samples
    for (uint i = 0; i < info.samples.size(); ++i) {
      // Assign values to matrix A
      for (uint j = 0; j < n_bary; ++j) {
        auto A = (info.basis * info.weights[i][j]).cast<double>().eval();
        params.A.block<rowcol(A)>(i * n_spec, j * n_base) = A;
      }

      // Assign values to matrix b
      auto b = info.samples[i].cast<double>().eval();
      params.b.block<rowcol(b)>(i * n_spec, 0) = b;
    }

    // Add constraints to ensure resulting spectra remain bounded near original gamut positions
    const auto gamut_csys = (info.system.transpose() * info.basis).cast<double>().eval();
    const uint l_gamut_offs = info.samples.size() * n_spec;
    for (uint i = 0; i < info.gamut.size(); ++i) {
      params.A.block<rowcol(gamut_csys)>(l_gamut_offs + i * n_colr, i * n_base) = gamut_csys;

      auto b = info.gamut[i].cast<double>().eval();
      params.b.block<rowcol(b)>(l_gamut_offs + i * n_colr, 0) = (b - gamut_err).max(0.0).eval();
    }
    params.r.block(l_gamut_offs, 0, info.gamut.size() * n_colr, 1) = LPCompare::eEQ;
    // params.r.block(u_gamut_offs, 0, info.gamut.size() * n_colr, 1) = LPCompare::eLE;

    // Add constraints to limit resulting spectra to [0, 1]
    const uint l_bounds_offs = l_gamut_offs + info.gamut.size() * n_colr;
    const uint u_bounds_offs = l_bounds_offs + n_bary * n_spec;
    const auto basis = info.basis.cast<double>().eval();
    for (uint i = 0; i < n_bary; ++i) {
      params.A.block<rowcol(basis)>(l_bounds_offs + i * n_spec, i * n_base) = basis;
      params.A.block<rowcol(basis)>(u_bounds_offs + i * n_spec, i * n_base) = basis;
    }
    params.b.block(l_bounds_offs, 0, n_bary * n_spec, 1) = 0.0;
    params.r.block(l_bounds_offs, 0, n_bary * n_spec, 1) = LPCompare::eGE;
    params.b.block(u_bounds_offs, 0, n_bary * n_spec, 1) = 1.0;
    params.r.block(u_bounds_offs, 0, n_bary * n_spec, 1) = LPCompare::eLE;

    // Run solver and pray; cast result back to float
    auto x = lp_solve(params).cast<float>().eval();

    // Obtain basis function weights from solution and compute resulting spectra
    std::vector<Spec> out(n_bary);
    for (uint i = 0; i < n_bary; ++i)
      out[i] = info.basis * BSpec(x.block<n_base, 1>(n_base * i, 0));
    
    return out;
  }

  std::vector<Spec> generate_gamut(const GenerateGamutInfo &info) {
    // Constant and type shorthands
    using Signal = GenerateGamutInfo::Signal;
    constexpr uint n_bary = barycentric_weights;
    constexpr uint n_spec = wavelength_samples;
    constexpr uint n_base = wavelength_bases;
    constexpr uint n_colr = 3;

    // Common matrix sizes
    constexpr uint N = n_bary * n_base;
    const     uint M = info.signals.size() * n_colr 
                     + info.gamut.size() * n_colr
                     + 2 * n_bary * n_spec;
    
    // Initialize parameter object for LP solver with expected matrix sizes
    LPParameters params(M, N);
    params.method    = LPMethod::ePrimal;
    params.scaling   = true;
    params.objective = LPObjective::eMinimize;

    // Clear untouched matrix values to 0
    params.A.fill(0.0);

    // Add constraints to ensure resulting spectra produce the given colors
    for (uint i = 0; i < info.signals.size(); ++i) {
      const Signal &sign = info.signals[i];
      for (uint j = 0; j < n_bary; ++j) {
        auto A = (info.systems[sign.syst_i].transpose() * info.basis * sign.bary_v[j]).cast<double>().eval();
        params.A.block(i * n_colr, j * n_base, rowcol(A)) = A;
      }
      
      auto b = sign.colr_v.cast<double>().eval();
      params.b.block(i * n_colr, 0, rowcol(b)) = b;
    }

    // Add constraints to ensure resulting spectra reproduce gamut positions exactly
    const auto gamut_csys = (info.systems[0].transpose() * info.basis).cast<double>().eval();
    const uint gamut_offs = info.signals.size() * n_colr;
    for (uint i = 0; i < info.gamut.size(); ++i) {
      auto b = info.gamut[i].cast<double>().eval();
      params.A.block(gamut_offs + i * n_colr, i * n_base, rowcol(gamut_csys)) = gamut_csys;
      params.b.block(gamut_offs + i * n_colr, 0, rowcol(b)) = b;
    }

    // Add constraints to limit resulting spectra to [0, 1]
    const uint l_offs = gamut_offs + info.gamut.size() * n_colr;
    const uint u_offs = l_offs + n_bary * n_spec;
    const auto basis = info.basis.cast<double>().eval();
    for (uint i = 0; i < n_bary; ++i) {
      params.A.block(l_offs + i * n_spec, i * n_base, rowcol(basis)) = basis;
      params.A.block(u_offs + i * n_spec, i * n_base, rowcol(basis)) = basis;
    }
    params.b.block(l_offs, 0, n_bary * n_spec, 1) = 0.0;
    params.b.block(u_offs, 0, n_bary * n_spec, 1) = 1.0;
    params.r.block(l_offs, 0, n_bary * n_spec, 1) = LPCompare::eGE;
    params.r.block(u_offs, 0, n_bary * n_spec, 1) = LPCompare::eLE;

    // Run solver and pray; cast result back to float
    auto x = lp_solve(params).cast<float>().eval();

    // Obtain basis function weights from solution and compute resulting spectra
    std::vector<Spec> out(n_bary);
    for (uint i = 0; i < n_bary; ++i)
      out[i] = info.basis * BSpec(x.block<n_base, 1>(n_base * i, 0));
    
    return out;
  }

  std::vector<Spec> generate_gamut(const BBasis            &basis,
                                   const std::vector<Wght> &weights,
                                   const std::vector<Colr> &samples,
                                   const CMFS              &sample_system) {
    debug::check_expr_rel(weights.size() == samples.size());

    const uint W = weights[0].size();
    const uint N = W * wavelength_bases;
    const uint M = 3 * samples.size()
                 + 2 * W * wavelength_samples;

    const auto csys = (sample_system.transpose() * basis).cast<double>().eval();

    // Initialize parameter object for LP solver with expected matrix sizes
    LPParameters params(M, N);
    params.method    = LPMethod::eDual;
    params.scaling   = true;
    params.objective = LPObjective::eMinimize;
    
    // Add constraints to ensure resulting spectra produce the given colors
    params.A.fill(0.0);
    for (uint i = 0; i < samples.size(); ++i) {
      debug::check_expr_rel(weights[i].size() == W);

      for (uint j = 0; j < W; ++j) {
        const auto wsys = (csys * weights[i][j]).eval();
        params.A.block(
          i * 3,
          j * wavelength_bases,
          3, 
          wavelength_bases
        ) = wsys;
      }

      eig::Vector3d b = samples[i].cast<double>().eval();
      params.b.block(i * 3, 0, 3, 1) = b;
    }

    // Add constraints to restrict resulting spectra are bounded to [0, 1]
    const uint offs_l = samples.size() * 3;
    const uint offs_u = offs_l + W * wavelength_samples;
    for (uint i = 0; i < W; ++i) {
      params.A.block<wavelength_samples, wavelength_bases>(
        offs_l + i * wavelength_samples, 
        i * wavelength_bases
      ) = basis.cast<double>().eval();
      params.A.block<wavelength_samples, wavelength_bases>(
        offs_u + i * wavelength_samples, 
        i * wavelength_bases
      ) = basis.cast<double>().eval();
    }
    params.b.block(offs_l, 0, W * wavelength_samples, 1) = 0.0;
    params.b.block(offs_u, 0, W * wavelength_samples, 1) = 1.0;
    params.r.block(offs_l, 0, W * wavelength_samples, 1) = LPCompare::eGE;
    params.r.block(offs_u, 0, W * wavelength_samples, 1) = LPCompare::eLE;

    // Run solver and pray
    auto x = lp_solve(params).cast<float>().eval();

    // fmt::print("x = {}\n", x);

    // Obtain basis function weights from solution
    std::vector<BSpec> output(W);
    for (uint i = 0; i < W; ++i)
      output[i] = x.block<wavelength_bases, 1>(wavelength_bases * i, 0);
    
    // Obtain actual spectra from basis function weights
    std::vector<Spec> _output(W);
    std::ranges::transform(output, _output.begin(),
      [&](const BSpec &b) { return (basis * b).eval(); });
    
    return _output;
  }
  
  
  std::vector<Spec> generate_gamut(const BBasis            &basis,
                                   const std::vector<Wght> &weights,
                                   const std::vector<Spec> &samples) {
    const uint W = weights[0].size();
    const uint N = W * wavelength_bases;
    const uint M = samples.size() * wavelength_samples 
                 + 2 * W * wavelength_samples;

    // Initialize parameter object for LP solver with expected matrix sizes
    LPParameters params(M, N);
    params.method    = LPMethod::ePrimal;
    params.scaling   = true;
    params.objective = LPObjective::eMinimize;

    // Add constraints to ensure resulting spectra produce the given spectral samples
    params.A.fill(0.0);
    for (uint i = 0; i < samples.size(); ++i) {
      auto w = eig::Map<const eig::VectorXf>(weights[i].data(), eig::Index(W)).cast<double>().eval();
      auto b = samples[i].cast<double>().eval();
      for (uint j = 0; j < W; ++j)
        params.A.block<wavelength_samples, wavelength_bases>(
          wavelength_samples * i, 
          wavelength_bases * j
        ) = eig::Vector<double, wavelength_samples>(w[j]).asDiagonal() * basis.cast<double>();
      params.b.block<wavelength_samples, 1>(wavelength_samples * i, 0) = b;
    }

    // Add constraints to restrict resulting spectra are bounded to [0, 1]
    const uint offs_l = samples.size() * wavelength_samples;
    const uint offs_u = offs_l + W * wavelength_samples;
    for (uint i = 0; i < W; ++i) {
      params.A.block<wavelength_samples, wavelength_bases>(
        offs_l + i * wavelength_samples, 
        i * wavelength_bases
      ) = basis.cast<double>().eval();
      params.A.block<wavelength_samples, wavelength_bases>(
        offs_u + i * wavelength_samples, 
        i * wavelength_bases
      ) = basis.cast<double>().eval();
    }
    params.b.block(offs_l, 0, W * wavelength_samples, 1) = 0.0;
    params.b.block(offs_u, 0, W * wavelength_samples, 1) = 1.0;
    params.r.block(offs_l, 0, W * wavelength_samples, 1) = LPCompare::eGE;
    params.r.block(offs_u, 0, W * wavelength_samples, 1) = LPCompare::eLE;

    // Run solver and pray
    auto x = lp_solve(params).cast<float>().eval();

    // Obtain basis function weights from solution
    std::vector<BSpec> output(W);
    for (uint i = 0; i < W; ++i)
      output[i] = x.block<wavelength_bases, 1>(wavelength_bases * i, 0);
    
    // Obtain actual spectra from basis function weights
    std::vector<Spec> _output(W);
    std::ranges::transform(output, _output.begin(),
      [&](const BSpec &b) { return (basis * b).eval(); });
    
    return _output;
  }

  std::vector<Colr> generate_gamut(const std::vector<Wght> &weights,
                                   const std::vector<Colr> &samples) {
    const uint W = weights[0].size();
    const uint N = 3 * W;
    const uint M = 3 * weights.size();

    // Initialize parameter object for LP solver with expected matrix sizes
    LPParameters params(M, N);
    params.method    = LPMethod::ePrimal;
    params.scaling   = true;
    params.objective = LPObjective::eMinimize;

    // Set solution boundaries
    params.x_l = 0.f;
    params.x_u = 1.f;

    // Specify constraints A*x = b
    params.A.fill(0.0);
    for (uint i = 0; i < samples.size(); ++i) {
      eig::VectorXd w = eig::Map<const eig::VectorXf>(weights[i].data(), eig::Index(W)).cast<double>();
      eig::Vector3d b = samples[i].cast<double>();
      for (uint j = 0; j < W; ++j)
        params.A.block<3, 3>(3 * i, 3 * j) = eig::Vector3d(w[j]).asDiagonal();
      params.b.block<3, 1>(3 * i, 0) = b; 
    }

    // Solve for original colors and separate these into return format
    auto x = lp_solve(params).cast<float>().eval();
    std::vector<Colr> v(W);
    for (uint i = 0; i < W; ++i)
      v[i] = x.block<3, 1>(3 * i, 0);
    return v;
  }
} // namespace met