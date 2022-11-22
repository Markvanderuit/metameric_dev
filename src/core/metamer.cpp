#include <metameric/core/linprog.hpp>
#include <metameric/core/metamer.hpp>
#include <metameric/core/utility.hpp>
#include <metameric/core/detail/trace.hpp>
#include <algorithm>
#include <execution>

namespace met {
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
    constexpr uint N = wavelength_bases, M = 3 + 2 * wavelength_samples;
    LPParameters params(M, N);
    params.method = LPMethod::eDual;

    // Specify constraints
    params.A = (eig::Matrix<float, M, N>() << csys_i, basis, basis).finished().cast<double>().eval();
    params.b = (eig::Matrix<float, M, 1>() << signal_i, Spec(0.f), Spec(1.f)).finished().cast<double>().eval();
    params.r.block<wavelength_samples, 1>(3, 0)                      = LPCompare::eGE;
    params.r.block<wavelength_samples, 1>(3 + wavelength_samples, 0) = LPCompare::eLE;

    // Obtain orthogonal basis functions through SVD of dual color system matrix
    eig::Matrix<float, N, 6> S = (eig::Matrix<float, N, 6>() << csys_i.transpose(), csys_j.transpose()).finished();
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

    return output;
  }
} // namespace met