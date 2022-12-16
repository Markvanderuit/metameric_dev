#include <metameric/core/linprog.hpp>
#include <metameric/core/metamer.hpp>
#include <metameric/core/utility.hpp>
#include <metameric/core/detail/trace.hpp>
#include <omp.h>
#include <fmt/ranges.h>
#include <algorithm>
#include <execution>
#include <unordered_set>

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
  
  std::vector<Colr> generate_boundary(const BBasis                               &basis,
                                      const CMFS                                 &system_i,
                                      const CMFS                                 &system_j,
                                      const Colr                                 &signal_i,
                                      const std::vector<eig::Array<float, 6, 1>> &samples) {
    met_trace();
    
    // Fixed color system spectra for basis parameters
    auto csys_i = (system_i.transpose() * basis).eval();
    auto csys_j = (system_j.transpose() * basis).eval();

    // Initialize parameter object for LP solver with expected matrix sizes
    constexpr uint N = wavelength_bases, 
                   M = 3 + 2 * wavelength_samples;
    LPParameters params(M, N);
    params.method = LPMethod::eDual;

    // Specify constraints
    params.A = (eig::Matrix<float, M, N>() << 
      csys_i, 
      basis, 
      basis
    ).finished().cast<double>().eval();
    params.b = (eig::Matrix<float, M, 1>() << 
      signal_i, 
      Spec(0.f), 
      Spec(1.f)
    ).finished().cast<double>().eval();
    params.r.block<wavelength_samples, 1>(3, 0)                      = LPCompare::eGE;
    params.r.block<wavelength_samples, 1>(3 + wavelength_samples, 0) = LPCompare::eLE;

    // Obtain orthogonal basis functions through SVD of dual color system matrix
    eig::Matrix<float, N, 6> S = (eig::Matrix<float, N, 6>() << 
      csys_i.transpose(), 
      csys_j.transpose()
    ).finished();
    eig::JacobiSVD<eig::Matrix<float, N, 6>> svd;
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

  
  std::vector<Colr> generate_gamut(const std::vector<Wght> &weights,
                                   const std::vector<Colr> &samples) {
    const uint W = weights[0].size();
    const uint M = 3 * weights.size();
    const uint N = 3 * W;

    // Initialize parameter object for LP solver with expected matrix sizes
    LPParameters params(M, N);
    params.method    = LPMethod::ePrimal;
    params.scaling   = true;
    params.objective = LPObjective::eMinimize;

    eig::ArrayXd A_(W, 1);
    std::ranges::copy(weights[0], A_.begin());

    // Specify constraints A*x = b
    params.A.fill(0.0);
    for (uint i = 0; i < weights.size(); ++i) {
      eig::ArrayXd A_(W, 1);
      std::ranges::copy(weights[i], A_.begin());
      
      params.A.block(3 * i + 0, 0,     1, W) = A_.transpose();
      params.A.block(3 * i + 1, W,     1, W) = A_.transpose();
      params.A.block(3 * i + 2, 2 * W, 1, W) = A_.transpose();
      params.b.block(3 * i,     0,     3, 1) = samples[i].cast<double>(); 
    }

    // Set boundary constraints
    // params.x_l =-.67f;
    // params.x_u = .67f;

    // Solve for original colors and separate these into return format
    auto x = lp_solve(params).cast<float>().eval();
    // fmt::print("A_ = {}\n", A_);
    // fmt::print("A = {}\n", params.A.row(0));
    // fmt::print("x = {}\n", x);
    // fmt::print("params.b = {}\n", params.b);
    std::vector<Colr> v(W);
    for (uint i = 0; i < W; ++i)
      v[i] = Colr(x[i], x[W + i], x[2 * W + i]);
    return v;
  }
} // namespace met