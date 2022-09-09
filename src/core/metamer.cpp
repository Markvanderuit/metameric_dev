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

    // Generate fundamental basis function weights
    BCMFS g = (csys_i.transpose() * basis_funcs).transpose().eval();
    BSpec r_fundm = g * (g.transpose() * g).inverse() * color_i.matrix();
    Spec  s_fundm = basis_funcs * r_fundm;

    // Given a specific color A under I
    // and a requested color B under J
    // and constraints that 0 <= gen(r_fundm + r_black) <= 1
    // and known r_fundm
    // solve for r_black
    


    auto A_lu = (basis_funcs * black_funcs).eval();
    auto b_l  = (-(basis_funcs * r_fundm)).eval();
    auto b_u  = (Spec(1.f).matrix() - (basis_funcs * r_fundm)).eval();
    // auto A_eq = (basis_funcs * black_funcs).eval();
    // auto b_eq = 

    auto A_equals = (r_fundm.transpose() * black_funcs).eval();
    // auto A_bounds = basis_funcs * black_funcs;

    // Generate (valid, bounded) metameric black weights using linear programming
    auto A = (csys_i.transpose() * basis_funcs * black_funcs).eval();
    LPParams<float, wavelength_blacks, 3> lp_params {
      .C = 0.f, 
      .A = A, 
      .b = Colr(0.f), 
      .r = LPComp::eEQ,
      .l =-1.f, 
      .u = 1.f
    };
    auto alpha = linprog<float, wavelength_blacks, 3>(lp_params);

    // auto  alpha   = eig::Matrix<float, wavelength_bases - 3, 1>::Random();
    // BSpec r_black = (0.2f * black_funcs * alpha).eval();
    fmt::print("r_black {}\n", alpha);

    // Return resulting spectrum by weighting basis functions 
    return (basis_funcs * (r_fundm)).cwiseMax(0.f).cwiseMin(1.f).eval();
  }
} // namespace met