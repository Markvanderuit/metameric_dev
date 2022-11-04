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

    // Expected matrix sizes
    constexpr uint N = wavelength_bases;
    const     uint M = 3 * systems.size() + 2 * wavelength_samples;
    
    // Constraints matrices
    eig::MatrixXd          A(M, N);
    eig::ArrayXd           b(M);
    eig::ArrayX<LPComp> r(M);

    // Generate color constraints
    for (uint i = 0; i < systems.size(); ++i) {
      A.block<3, wavelength_bases>(3 * i, 0) = (systems[i].transpose() * basis).cast<double>().eval();
      b.block<3, 1>(3 * i, 0) = signals[i].cast<double>().eval();
      r.block<3, 1>(3 * i, 0).setConstant(LPComp::eEQ);
    }

    // Generate boundary constraints
    const uint offset_l = 3 * systems.size();
    const uint offset_u = offset_l + wavelength_samples;
    A.block<wavelength_samples, wavelength_bases>(offset_l, 0) = basis.cast<double>().eval();
    b.block<wavelength_samples, 1>(offset_l, 0).setZero();
    r.block<wavelength_samples, 1>(offset_l, 0).setConstant(LPComp::eGE);
    A.block<wavelength_samples, wavelength_bases>(offset_u, 0) = basis.cast<double>().eval();
    b.block<wavelength_samples, 1>(offset_u, 0).setOnes();
    r.block<wavelength_samples, 1>(offset_u, 0).setConstant(LPComp::eLE);

    // Objective matrices for minimization/maximization of x
    eig::Array<double, N, 1> C_min = 1.0, C_max =-1.0;

    // Upper and lower limits to x are unrestrained
    eig::Array<float, N, 1> l = std::numeric_limits<float>::min(), u = std::numeric_limits<float>::max();

    // Upper and lower limits to x
    eig::Array<double, N, 1> x_l = lp_min_value;
    eig::Array<double, N, 1> x_u = lp_max_value;

    // Set up full set of parameters for solving minimized/maximized weights
    /* LPParameters lp_params_tin {
      .method = LPMethod::eDual,
      .M = M, .N = N, .C = C_min, .A = A, .b = b, .r = r, .x_l = x_l, .x_u = x_u
    };
    LPParameters lp_params_tax {
      .method = LPMethod::eDual,
      .M = M, .N = N, .C = C_max, .A = A, .b = b, .r = r, .x_l = x_l, .x_u = x_u
    }; */

    /* // Set up full set of parameters for solving minimized/maximized weights
    LPParamsX<float> lp_params_min { .N = N, .M = M, .C = C_min, 
                                     .A = A, .b = b, .c0 = 0.f, 
                                     .r = r, .l = l, .u = u };
    LPParamsX<float> lp_params_max { .N = N, .M = M, .C = C_max, 
                                     .A = A, .b = b, .c0 = 0.f, 
                                     .r = r, .l = l, .u = u };
     */
    LPParamsX<double> lp_params_test = { .N = N, .M = M, .C = C_min.cast<double>(),
                                         .A = A.cast<double>(), .b = b.cast<double>(), .c0 = 0.0,
                                         .r = r, .l = l.cast<double>(), .u = u.cast<double>() };
    LPParamsX<double> lp_params_test_ = { .N = N, .M = M, .C = C_max.cast<double>(),
                                         .A = A.cast<double>(), .b = b.cast<double>(), .c0 = 0.0,
                                         .r = r, .l = l.cast<double>(), .u = u.cast<double>() };
    BSpec w_test_min = linprog_test<double>(lp_params_test).cast<float>();
    BSpec w_test_max = linprog_test<double>(lp_params_test_).cast<float>();


    // Take average of minimized/maximized results
    // BSpec w = 0.5f * linprog<float>(lp_params_min) + 0.5f * linprog<float>(lp_params_max);
    // BSpec w = (0.5 * lp_solve(lp_params_tin) + 0.5 * lp_solve(lp_params_tax)).cast<float>();
    BSpec w = 0.5f * w_test_min + 0.5f * w_test_max;
    return (basis * w).eval();
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

    // Obtain orthogonal basis functions through SVD of dual color system matrix
    auto S = (eig::Matrix<float, wavelength_bases, 6>() << csys_i.transpose(), csys_j.transpose()).finished();
    eig::JacobiSVD<decltype(S)> svd(S, eig::ComputeThinU | eig::ComputeThinV);
    auto U = (S * svd.matrixV() * svd.singularValues().asDiagonal().inverse()).eval();

    // Constraints matrices
    constexpr uint N = wavelength_bases, M = 3 + 2 * wavelength_samples;
    auto A = (eig::Matrix<float, M, N>() << csys_i, basis, basis).finished();
    auto b = (eig::Matrix<float, M, 1>() << signal_i, Spec(0.f), Spec(1.f)).finished();
    auto r = (eig::Matrix<LPComp, M, 1>() 
      << eig::Matrix<LPComp, 3, 1>(LPComp::eEQ),
         eig::Matrix<LPComp, wavelength_samples, 1>(LPComp::eGE),
         eig::Matrix<LPComp, wavelength_samples, 1>(LPComp::eLE)).finished();
    
    // Define return object
    std::vector<Colr> output(samples.size());

    // Parallel solve for basis function weights defining OCS boundary spectra
    #pragma omp parallel
    {
      LPParams<float, N, M> params = { .A = A, .b = b, .c0 = 0.f, .r = r };
      #pragma omp for
      for (int i = 0; i < samples.size(); ++i) {
        params.C = (U * samples[i].matrix()).eval();
        output[i] = (csys_j * linprog<float, N, M>(params)).eval();
      }
    }

    return output;
  }
} // namespace met