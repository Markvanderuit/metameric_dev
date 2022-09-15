#include <metameric/core/linprog.hpp>
#include <metameric/core/metamer.hpp>
#include <metameric/core/utility.hpp>
#include <metameric/core/detail/trace.hpp>
#include <algorithm>
#include <execution>

namespace met {
  // Spec MetamerMapping::generate(const Colr &color_i,
  //                               const Colr &color_j) {
  //   met_trace();

  //   // Color system spectra
  //   CMFS csys_i = mapping_i.finalize();
  //   CMFS csys_j = mapping_j.finalize();

  //   BCMFS G_i = (csys_i.transpose() * basis_funcs).transpose().eval();
  //   BCMFS G_j = (csys_j.transpose() * basis_funcs).transpose().eval();

  //   // Generate fundamental basis function weights
  //   BSpec r_fundm;
  //   {
  //     // r_fundm = G_i * (G_i.transpose() * G_i).inverse() * color_i.matrix();

  //     // Generate color constraints
  //     /* 3 x K */ auto A_i = (csys_i.transpose() * basis_funcs).eval();
  //     /* 3 x 1 */ auto b_i = color_i;
  //     /* 3 x 1 */ auto r_i = eig::Matrix<LPComp, 3, 1>(LPComp::eEQ);

  //     // Generate boundary constraints for reflectance spectrum
  //     /* N x K */ auto A_lu = basis_funcs;
  //     /* N x 1 */ auto b_l = Spec(0.f);
  //     /* N x 1 */ auto b_u = Spec(1.f);
  //     /* 3 x 1 */ auto r_l = eig::Matrix<LPComp, wavelength_samples, 1>(LPComp::eGE);
  //     /* 3 x 1 */ auto r_u = eig::Matrix<LPComp, wavelength_samples, 1>(LPComp::eLE);

  //     // Set up constraint matrices
  //     constexpr uint N = wavelength_bases;
  //     constexpr uint M = 3 + wavelength_samples + wavelength_samples;
  //     auto A = (eig::Matrix<float, M, N>()  << A_i, A_lu, A_lu).finished();
  //     auto b = (eig::Matrix<float, M, 1>()  << b_i, b_l, b_u).finished();
  //     auto r = (eig::Matrix<LPComp, M, 1>() << r_i, r_l, r_u).finished();

  //     // Set up full set of parameters and solve for alpha
  //     LPParams<float, N, M> lp_params { .C = 1.f, .A = A, .b = b, .c0 = 0.f, .r = r };
  //     r_fundm = linprog<float, N, M>(lp_params);
  //   }

  //   // Generate metameric black function weights
  //   BSpec r_black;
  //   {
  //     // Generate color constraints for i, j
  //     /* 3 x K - 3 */ auto A_i = (csys_i.transpose() * basis_funcs * black_funcs).eval();
  //     /* 3 x K - 3 */ auto A_j = (csys_j.transpose() * basis_funcs * black_funcs).eval();
  //     /* 3 x 1 */     auto b_i = (color_i - (G_i.transpose() * r_fundm).array()).eval();
  //     /* 3 x 1 */     auto b_j = (color_j - (G_j.transpose() * r_fundm).array()).eval();
  //     /* 3 x 1 */     auto r_i = eig::Matrix<LPComp, 3, 1>(LPComp::eEQ);
  //     /* 3 x 1 */     auto r_j = eig::Matrix<LPComp, 3, 1>(LPComp::eEQ);

  //     // Generate boundary constraints for reflectance spectrum
  //     /* N x K - 3 */ auto A_lu = (basis_funcs * black_funcs).eval();
  //     /* N x 1 */     auto b_l = (-(basis_funcs * r_fundm)).eval();
  //     /* N x 1 */     auto b_u = (Spec(1.f).matrix() - (basis_funcs * r_fundm)).eval();
  //     /* N x 1 */     auto r_l = eig::Matrix<LPComp, wavelength_samples, 1>(LPComp::eGE);
  //     /* N x 1 */     auto r_u = eig::Matrix<LPComp, wavelength_samples, 1>(LPComp::eLE);

  //     // Set up constraints matrices
  //     constexpr uint N = wavelength_blacks;
  //     constexpr uint M = 3 + 3 + wavelength_samples + wavelength_samples;
  //     auto A = (eig::Matrix<float, M, N>()  << A_i, A_j, A_lu, A_lu).finished();
  //     auto b = (eig::Matrix<float, M, 1>()  << b_i, b_j, b_l, b_u).finished();
  //     auto r = (eig::Matrix<LPComp, M, 1>() << r_i, r_j, r_l, r_u).finished();
      
  //     // Set up full set of parameters and solve for alpha
  //     LPParams<float, N, M> lp_params { .C = 0.f, .A = A, .b = b, .c0 = 0.f, .r = r };
  //     auto alpha = linprog<float, N, M>(lp_params);

  //     // Generate metameric black weights from alpha
  //     r_black = (black_funcs * alpha).eval();
  //   }
        
  //   // Generate spectral distribution by weighting basis functions
  //   return (basis_funcs * (r_fundm + r_black)); //s.cwiseMax(0.f).cwiseMin(1.f).eval();
  // }

  // Spec MetamerMapping::generate(const std::vector<Colr> &constraints) {
  //   // Color system spectra
  //   CMFS csys_i = mapping_i.finalize();
  //   CMFS csys_j = mapping_j.finalize();

  //   // Generate basis function weights satisfying all constraints
  //   BSpec w;
  //   {
  //     // Generate color constraints
  //     auto A_i = (csys_i.transpose() * basis_funcs).eval();
  //     auto A_j = (csys_j.transpose() * basis_funcs).eval();
  //     auto b_i = constraints[0];
  //     auto b_j = constraints[1];
  //     auto r_i = eig::Matrix<LPComp, 3, 1>(LPComp::eEQ);
  //     auto r_j = eig::Matrix<LPComp, 3, 1>(LPComp::eEQ);

  //     // Generate boundary constraints for reflectance spectrum
  //     /* N x K */ auto A_lu = basis_funcs;
  //     /* N x 1 */ auto b_l = Spec(0.f);
  //     /* N x 1 */ auto b_u = Spec(1.f);
  //     /* 3 x 1 */ auto r_l = eig::Matrix<LPComp, wavelength_samples, 1>(LPComp::eGE);
  //     /* 3 x 1 */ auto r_u = eig::Matrix<LPComp, wavelength_samples, 1>(LPComp::eLE);
      
  //     // Set up constraint matrices
  //     constexpr uint N = wavelength_bases;
  //     constexpr uint M = 3 + wavelength_samples + wavelength_samples;
  //     auto A = (eig::Matrix<float, M, N>()  << A_i, A_lu, A_lu).finished();
  //     auto b = (eig::Matrix<float, M, 1>()  << b_i, b_l, b_u).finished();
  //     auto r = (eig::Matrix<LPComp, M, 1>() << r_i, r_l, r_u).finished();

  //     // Set up full set of parameters for solving minimized/maximized weights
  //     LPParams<float, N, M> lp_params_min { .C = 1.f, .A = A, .b = b, .c0 = 0.f, .r = r };
  //     LPParams<float, N, M> lp_params_max { .C =-1.f, .A = A, .b = b, .c0 = 0.f, .r = r };

  //     // Take average of minimized/maximized spectra
  //     w = 0.5f * linprog<float, N, M>(lp_params_min) 
  //       + 0.5f * linprog<float, N, M>(lp_params_max);
  //   }

  //   return (basis_funcs * w).eval();
  // }
  
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
    eig::MatrixXf       A(M, N);
    eig::ArrayXf        b(M);
    eig::ArrayX<LPComp> r(M);

    // Generate color constraints
    for (uint i = 0; i < systems.size(); ++i) {
      A.block<3, wavelength_bases>(3 * i, 0) = (systems[i].transpose() * basis).eval();
      b.block<3, 1>(3 * i, 0) = signals[i];
      r.block<3, 1>(3 * i, 0).setConstant(LPComp::eEQ);
    }

    // Generate boundary constraints
    const uint offset_l = 3 * systems.size();
    const uint offset_u = offset_l + wavelength_samples;
    A.block<wavelength_samples, wavelength_bases>(offset_l, 0) = basis;
    b.block<wavelength_samples, 1>(offset_l, 0).setZero();
    r.block<wavelength_samples, 1>(offset_l, 0).setConstant(LPComp::eGE);
    A.block<wavelength_samples, wavelength_bases>(offset_u, 0) = basis;
    b.block<wavelength_samples, 1>(offset_u, 0).setOnes();
    r.block<wavelength_samples, 1>(offset_u, 0).setConstant(LPComp::eLE);

    // Objective matrices for minimization/maximization of x
    eig::Array<float, N, 1> C_min = 1.f, C_max =-1.f;

    // Upper and lower limits to x are unrestrained
    eig::Array<float, N, 1> l = std::numeric_limits<float>::min(), u = std::numeric_limits<float>::max();

      // Set up full set of parameters for solving minimized/maximized weights
    LPParamsX<float> lp_params_min { .N = N, .M = M, .C = C_min, 
                                     .A = A, .b = b, .c0 = 0.f, 
                                     .r = r, .l = l, .u = u };
    LPParamsX<float> lp_params_max { .N = N, .M = M, .C = C_max, 
                                     .A = A, .b = b, .c0 = 0.f, 
                                     .r = r, .l = l, .u = u };

    // Take average of minimized/maximized results
    BSpec w = 0.5f * linprog<float>(lp_params_min) + 0.5f * linprog<float>(lp_params_max);

    return (basis * w).eval();
  }
  
  std::vector<Spec> generate_boundary_spec(const BBasis                               &basis,
                                           const CMFS                                 &system_i,
                                           const CMFS                                 &system_j,
                                           const Colr                                 &signal_i,
                                           const std::vector<eig::Array<float, 6, 1>> &samples) {
    met_trace();

    // Obtain six orthogonal spectra through SVD of dual color system matrix
    auto S = (eig::Matrix<float, wavelength_samples, 6>() << system_i, system_j).finished();
    eig::JacobiSVD<decltype(S)> svd(S, eig::ComputeThinU | eig::ComputeThinV);
    auto U  = (S * svd.matrixV() * svd.singularValues().asDiagonal().inverse()).eval();
    auto U_ = (U.transpose() * basis).eval(); // multiplied by basis  functions

    // Fixed color system spectra for basis parameters
    auto csys_i = (system_i.transpose() * basis).eval();
    auto csys_j = (system_j.transpose() * basis).eval();

    // Constraints matrices
    constexpr uint N = wavelength_bases, M = 3 + 2 * wavelength_samples;
    auto A = (eig::Matrix<float, M, N>() << csys_i, basis, basis).finished();
    auto b = (eig::Matrix<float, M, 1>() << signal_i, Spec(0.f), Spec(1.f)).finished();
    auto r = (eig::Matrix<LPComp, M, 1>() 
      << eig::Matrix<LPComp, 3, 1>(LPComp::eEQ),
         eig::Matrix<LPComp, wavelength_samples, 1>(LPComp::eGE),
         eig::Matrix<LPComp, wavelength_samples, 1>(LPComp::eLE)).finished();
    
    // Return object
    std::vector<Spec> output(samples.size());

    // Parallel solve for basis function weights defining OCS boundary spectra
    #pragma omp parallel
    {
      LPParams<float, N, M> params = { .A = A, .b = b, .c0 = 0.f, .r = r, };

      #pragma omp for
      for (int i = 0; i < samples.size(); ++i) {
        params.C = (U_.transpose() * samples[i].matrix()).eval();
        output[i] = (basis * linprog<float, N, M>(params)).eval();
      }
    }

    return output;
  }

  std::vector<Colr> generate_boundary_colr(const BBasis                               &basis,
                                           const CMFS                                 &system_i,
                                           const CMFS                                 &system_j,
                                           const Colr                                 &signal_i,
                                           const std::vector<eig::Array<float, 6, 1>> &samples) {
    met_trace();
    std::vector<Spec> input = generate_boundary_spec(basis, system_i, system_j, signal_i, samples);
    std::vector<Colr> output(samples.size());
    std::transform(std::execution::par_unseq, range_iter(input), output.begin(),
      [&](const auto &s) { return (system_j.transpose() * s.matrix()).eval(); });
    return output;
  }
} // namespace met