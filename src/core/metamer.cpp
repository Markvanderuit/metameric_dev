#include <metameric/core/linprog.hpp>
#include <metameric/core/metamer.hpp>
#include <metameric/core/detail/trace.hpp>

namespace met {
  Spec MetamerMapping::generate(const Colr &color_i,
                                const Colr &color_j) {
    met_trace();

    // Color system spectra
    CMFS csys_i = mapping_i.finalize();
    CMFS csys_j = mapping_j.finalize();

    BCMFS G_i = (csys_i.transpose() * basis_funcs).transpose().eval();
    BCMFS G_j = (csys_j.transpose() * basis_funcs).transpose().eval();

    // Generate fundamental basis function weights
    BSpec r_fundm;
    {
      // r_fundm = G_i * (G_i.transpose() * G_i).inverse() * color_i.matrix();

      // Generate color constraints
      /* 3 x K */ auto A_i = (csys_i.transpose() * basis_funcs).eval();
      /* 3 x 1 */ auto b_i = color_i;
      /* 3 x 1 */ auto r_i = eig::Matrix<LPComp, 3, 1>(LPComp::eEQ);

      // Generate boundary constraints for reflectance spectrum
      /* N x K */ auto A_lu = basis_funcs;
      /* N x 1 */ auto b_l = Spec(0.f);
      /* N x 1 */ auto b_u = Spec(1.f);
      /* 3 x 1 */ auto r_l = eig::Matrix<LPComp, wavelength_samples, 1>(LPComp::eGE);
      /* 3 x 1 */ auto r_u = eig::Matrix<LPComp, wavelength_samples, 1>(LPComp::eLE);

      // Set up constraint matrices
      constexpr uint N = wavelength_bases;
      constexpr uint M = 3 + wavelength_samples + wavelength_samples;
      auto A = (eig::Matrix<float, M, N>()  << A_i, A_lu, A_lu).finished();
      auto b = (eig::Matrix<float, M, 1>()  << b_i, b_l, b_u).finished();
      auto r = (eig::Matrix<LPComp, M, 1>() << r_i, r_l, r_u).finished();

      // Set up full set of parameters and solve for alpha
      LPParams<float, N, M> lp_params { .C = 1.f, .A = A, .b = b, .c0 = 0.f, .r = r };
      r_fundm = linprog<float, N, M>(lp_params);
    }

    // Generate metameric black function weights
    BSpec r_black;
    {
      // Generate color constraints for i, j
      /* 3 x K - 3 */ auto A_i = (csys_i.transpose() * basis_funcs * black_funcs).eval();
      /* 3 x K - 3 */ auto A_j = (csys_j.transpose() * basis_funcs * black_funcs).eval();
      /* 3 x 1 */     auto b_i = (color_i - (G_i.transpose() * r_fundm).array()).eval();
      /* 3 x 1 */     auto b_j = (color_j - (G_j.transpose() * r_fundm).array()).eval();
      /* 3 x 1 */     auto r_i = eig::Matrix<LPComp, 3, 1>(LPComp::eEQ);
      /* 3 x 1 */     auto r_j = eig::Matrix<LPComp, 3, 1>(LPComp::eEQ);

      // Generate boundary constraints for reflectance spectrum
      /* N x K - 3 */ auto A_lu = (basis_funcs * black_funcs).eval();
      /* N x 1 */     auto b_l = (-(basis_funcs * r_fundm)).eval();
      /* N x 1 */     auto b_u = (Spec(1.f).matrix() - (basis_funcs * r_fundm)).eval();
      /* N x 1 */     auto r_l = eig::Matrix<LPComp, wavelength_samples, 1>(LPComp::eGE);
      /* N x 1 */     auto r_u = eig::Matrix<LPComp, wavelength_samples, 1>(LPComp::eLE);

      // Set up constraints matrices
      constexpr uint N = wavelength_blacks;
      constexpr uint M = 3 + 3 + wavelength_samples + wavelength_samples;
      auto A = (eig::Matrix<float, M, N>()  << A_i, A_j, A_lu, A_lu).finished();
      auto b = (eig::Matrix<float, M, 1>()  << b_i, b_j, b_l, b_u).finished();
      auto r = (eig::Matrix<LPComp, M, 1>() << r_i, r_j, r_l, r_u).finished();
      
      // Set up full set of parameters and solve for alpha
      LPParams<float, N, M> lp_params { .C = 0.f, .A = A, .b = b, .c0 = 0.f, .r = r };
      auto alpha = linprog<float, N, M>(lp_params);

      // Generate metameric black weights from alpha
      r_black = (black_funcs * alpha).eval();
    }
        
    // Generate spectral distribution by weighting basis functions
    // Spec s = (basis_funcs * (r_fundm)).cwiseMax(0.f).cwiseMin(1.f).eval();
    return (basis_funcs * (r_fundm + r_black)); //s.cwiseMax(0.f).cwiseMin(1.f).eval();
    // return s;
  }
} // namespace met