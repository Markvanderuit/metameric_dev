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

  Spec generate_spectrum(GenerateSpectrumInfo info) {
    met_trace();
    debug::check_expr_dbg(info.systems.size() == info.signals.size(),
                          "Color system size not equal to color signal size");

    // Initialize parameter object for LP solver with expected matrix sizes M, N
    constexpr uint N = wavelength_bases;
    const     uint M = 3 * info.systems.size() 
                     + (info.impose_boundedness ? 2 * wavelength_samples : 0);
    LPParameters params(M, N);
    params.method  = LPMethod::ePrimal;
    params.scaling = true;
    // params.C       = (info.systems[0].transpose() * info.basis).row(1).cast<double>().eval();

    // Add constraints to ensure resulting spectra produce the given color signals
    for (uint i = 0; i < info.systems.size(); ++i) {
      params.A.block<3, N>(3 * i, 0) = (info.systems[i].transpose() * info.basis).cast<double>().eval();
      params.b.block<3, 1>(3 * i, 0) = info.signals[i].cast<double>().eval();
    }

    // Add constraints to ensure resulting spectra are bounded to [0, 1]
    if (info.impose_boundedness) {
      const uint offs_l = 3 * info.systems.size();
      const uint offs_u = offs_l + wavelength_samples;
      params.A.block<wavelength_samples, N>(offs_l, 0) = info.basis.cast<double>().eval();
      params.A.block<wavelength_samples, N>(offs_u, 0) = info.basis.cast<double>().eval();
      params.b.block<wavelength_samples, 1>(offs_l, 0) = 0.0;
      params.b.block<wavelength_samples, 1>(offs_u, 0) = 1.0;
      params.r.block<wavelength_samples, 1>(offs_l, 0) = LPCompare::eGE;
      params.r.block<wavelength_samples, 1>(offs_u, 0) = LPCompare::eLE;
    }

    // Solve for minimized/maximized results and take average
    params.objective = LPObjective::eMinimize;
    BSpec minv = lp_solve(params).cast<float>();
    params.objective = LPObjective::eMaximize;
    BSpec maxv = lp_solve(params).cast<float>();
    return info.basis * 0.5f * (minv + maxv);
  }

  std::vector<Colr> generate_ocs_boundary(const GenerateOCSBoundaryInfo &info) {
    std::vector<Colr> out(info.samples.size());

    std::transform(std::execution::par_unseq, range_iter(info.samples), out.begin(), [&](const Colr &sample) {
      Spec s = (info.system * sample.matrix()).eval();
      for (auto &f : s)
        f = f >= 0.f ? 1.f : 0.f;

      // Find nearest generalized spectrum that fits within the basis function approach
      Spec s_ = generate_spectrum({
        .basis = info.basis,
        .systems = std::vector<CMFS> { info.system },
        .signals = std::vector<Colr> { (info.system.transpose() * s.matrix()).eval() }
      });

      return (info.system.transpose() * s_.matrix()).eval();
      // return (info.system.transpose() * s.matrix()).eval();
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
    constexpr uint N = wavelength_bases;
    const     uint M = 3 * csys_i.size() + 2 * wavelength_samples;
    LPParameters params(M, N);
    params.objective = LPObjective::eMaximize;
    params.method    = LPMethod::eDual;
    params.scaling   = true;

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

    // Define return object
    std::vector<Colr> output(info.samples.size());

    // Parallel solve for basis function weights defining OCS boundary spectra
    #pragma omp parallel
    {
      LPParameters local_params = params;
      #pragma omp for
      for (int i = 0; i < info.samples.size(); ++i) {
        local_params.C = (U * info.samples[i].matrix()).cast<double>().eval();
        BSpec w = lp_solve(local_params).cast<float>().eval();
        output[i] = csys_j * w;
      }
    }

    return detail::remove_identical_points(output);
  }

  // row/col expansion shorthand for a given eigen matrix
  #define rowcol(mat) decltype(mat)::RowsAtCompileTime, decltype(mat)::ColsAtCompileTime

  std::vector<Colr> generate_gamut(const GenerateGamutSimpleInfo &info) {
    const uint n_bary = info.bary_weights;
    const uint n_colr = 3;
    const uint N = n_bary * n_colr;
    const uint M = info.samples.size() * n_colr;
    
    // Initialize parameter object for LP solver with expected matrix sizes
    LPParameters params(M, N);
    params.method    = LPMethod::ePrimal;
    params.scaling   = true;
    params.objective = LPObjective::eMinimize;

    // Clear untouched matrix values to 0
    params.A.fill(0.0);

    // Add constraints to ensure weighted combinations produce the given samples
    for (uint i = 0; i < info.samples.size(); ++i) {
      for (uint j = 0; j < n_bary; ++j) {
        double w = info.weights[i][j];
        auto A = (eig::Matrix3d() << w, 0, 0, 0, w, 0, 0, 0, w).finished();
        fmt::print("A = {}\n", A.reshaped());
        params.A.block(i * n_colr, j * n_colr, rowcol(A)) = A;
      }
      auto b = info.samples[i].cast<double>().eval();
      fmt::print("b = {}\n", b.reshaped());
      params.b.block(i * n_colr, 0, rowcol(b)) = b;
    }

    // Run solver and pray; cast results back to float
    params.objective = LPObjective::eMinimize;
    auto x_min = lp_solve(params).cast<float>().eval();
    // params.objective = LPObjective::eMaximize;
    // auto x_max = lp_solve(params).cast<float>().eval();

    // Obtain resulting set of colors
    std::vector<Colr> out(n_bary);
    for (uint i = 0; i < n_bary; ++i)
      out[i] = x_min.block<n_colr, 1>(i * n_colr, 0);
      // out[i] = 0.5 * (x_min.block<n_colr, 1>(i * n_colr, 0) + x_max.block<n_colr, 1>(i * n_colr, 0));

    return out;
  } 

  std::vector<Spec> generate_gamut(const GenerateGamutSpectrumInfo &info) {
    // Constant shorthands
    constexpr uint n_bary = barycentric_weights;
    constexpr uint n_spec = wavelength_samples;
    constexpr uint n_base = wavelength_bases;
    constexpr uint n_colr = 3;
    constexpr double gamut_err = 0.0; //5;
    
    // Common matrix sizes
    constexpr uint N = n_bary * n_base;
    const     uint M = info.samples.size() * n_spec // Recovery constraints for seed samples
                     + info.gamut.size() * n_colr   // Roundtrip constraints for vertex positions
                     + 2 * n_bary * n_spec;         // Boundedness constraints for vertex spectra
    
    // Initialize parameter object for LP solver with expected matrix sizes
    LPParameters params(M, N);
    params.method    = LPMethod::eDual;
    params.scaling   = true;
    params.objective = LPObjective::eMinimize;

    // Clear untouched matrix values to 0
    params.A.fill(0.0);
    
    // Add recovery constraints for seed samples
    for (uint i = 0; i < info.samples.size(); ++i) {
      for (uint j = 0; j < n_bary; ++j) {
        auto A = (info.basis * info.weights[i][j]).cast<double>().eval();
        params.A.block(i * n_spec, j * n_base, rowcol(A)) = A;
      }

      auto b = info.samples[i].cast<double>().eval();
      params.b.block(i * n_spec, 0, rowcol(b)) = b;
    }

    // Add roundtrip constraints for gamut vertex positions
    const auto gamut_csys = (info.system.transpose() * info.basis).cast<double>().eval();
    const uint gamut_offs = info.samples.size() * n_spec;
    for (uint i = 0; i < info.gamut.size(); ++i) {
      auto b = info.gamut[i].cast<double>().eval();
      params.A.block(gamut_offs + i * n_colr, i * n_base, rowcol(gamut_csys)) = gamut_csys;
      params.b.block(gamut_offs + i * n_colr, 0, rowcol(b)) = (b - gamut_err).max(0.0).eval();
    }

    // Add boundedness constraints for resulting spectra
    const uint l_offs = gamut_offs + info.gamut.size() * n_colr;
    const uint u_offs = l_offs + n_bary * n_spec;
    const auto basis = info.basis.cast<double>().eval();
    for (uint i = 0; i < n_bary; ++i) {
      params.A.block<rowcol(basis)>(l_offs + i * n_spec, i * n_base) = basis;
      params.A.block<rowcol(basis)>(u_offs + i * n_spec, i * n_base) = basis;
    }
    params.b.block(l_offs, 0, n_bary * n_spec, 1) = 0.0;
    params.b.block(u_offs, 0, n_bary * n_spec, 1) = 1.0;
    params.r.block(l_offs, 0, n_bary * n_spec, 1) = LPCompare::eGE;
    params.r.block(u_offs, 0, n_bary * n_spec, 1) = LPCompare::eLE;

    // Run solver and pray; cast result back to float
    params.objective = LPObjective::eMinimize;
    auto x_min = lp_solve(params).cast<float>().eval();
    params.objective = LPObjective::eMaximize;
    auto x_max = lp_solve(params).cast<float>().eval();

    // Obtain basis function weights from solution and compute resulting spectra
    std::vector<Spec> out(n_bary);
    for (uint i = 0; i < n_bary; ++i) {
      out[i] = (0.5 * (
        info.basis * BSpec(x_min.block<n_base, 1>(n_base * i, 0)) + 
        info.basis * BSpec(x_max.block<n_base, 1>(n_base * i, 0))
      )).cwiseMax(0.f).cwiseMin(1.f).eval();
    }
    
    return out;
  }

  std::vector<Spec> generate_gamut(const GenerateGamutConstraintInfo &info) {
    // Constant and type shorthands
    using Signal = GenerateGamutConstraintInfo::Signal;
    constexpr uint n_bary = barycentric_weights;
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

    // Clear untouched matrix values to 0
    params.A.fill(0.0);

    // Specify minimization as Y sensitivity curve
    const auto gamut_csys = (info.systems[0].transpose() * info.basis).cast<double>().eval();
    // const auto gamut_copt = gamut_csys.row(1).transpose().eval();
    // for (uint i = 0; i <  n_bary; ++i)
    //   params.C.block(i * n_base, 0, rowcol(gamut_copt)) = gamut_copt;

    // Add roundtrip constraints for seed samples
    for (uint i = 0; i < info.signals.size(); ++i) {
      const Signal &sign = info.signals[i];
      for (uint j = 0; j < n_bary; ++j) {
        auto A = (info.systems[sign.syst_i].transpose() * info.basis * sign.bary_v[j]).cast<double>().eval();
        params.A.block(i * n_colr, j * n_base, rowcol(A)) = A;
      }
      
      auto b = sign.colr_v.cast<double>().eval();
      params.b.block(i * n_colr, 0, rowcol(b)) = b;
    }

    // Add roundtrip constraints for gamut vertex positions
    const uint gamut_offs = info.signals.size() * n_colr;
    for (uint i = 0; i < info.gamut.size(); ++i) {
      auto b = info.gamut[i].cast<double>().eval();
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
    }
    params.b.block(l_offs, 0, n_bary * n_spec, 1) = 0.0;
    params.b.block(u_offs, 0, n_bary * n_spec, 1) = 1.0;
    params.r.block(l_offs, 0, n_bary * n_spec, 1) = LPCompare::eGE;
    params.r.block(u_offs, 0, n_bary * n_spec, 1) = LPCompare::eLE;

    // Run solver and pray; cast results back to float
    params.objective = LPObjective::eMinimize;
    auto x_min = lp_solve(params).cast<float>().eval();
    params.objective = LPObjective::eMaximize;
    auto x_max = lp_solve(params).cast<float>().eval();

    // Obtain basis function weights from solution and compute resulting spectra
    std::vector<Spec> out(n_bary);
    for (uint i = 0; i < n_bary; ++i)
      out[i] = (info.basis * BSpec(x_min.block<n_base, 1>(n_base * i, 0))).cwiseMax(0.f).cwiseMin(1.f).eval();
    return out;
  }
} // namespace met