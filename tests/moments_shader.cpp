#include <catch2/catch_test_macros.hpp>
#include <metameric/core/constraints.hpp>
#include <metameric/core/matching.hpp>
#include <metameric/core/math.hpp>
#include <metameric/core/moments.hpp>
#include <metameric/core/ranges.hpp>
#include <metameric/core/metamer.hpp>
#include <metameric/core/spectrum.hpp>
#include <metameric/core/json.hpp>
#include <metameric/core/utility.hpp>
#include <metameric/core/tree.hpp>
#include <metameric/core/detail/packing.hpp>
#include <nlohmann/json.hpp>
#include <small_gl/window.hpp>
#include <small_gl/program.hpp>
#include <small_gl/buffer.hpp>
#include <small_gl/dispatch.hpp>
#include <small_gl/utility.hpp>
#include <fmt/core.h>
#include <fmt/ranges.h>
#include <complex>
#include <numbers>
#include <string_view>

TEST_CASE("Interpolation") {
  using namespace met;

  /* float a = 0.5f;
  eig::Array2f va = { 0.75, 0.25 };
  eig::Array2f vb = { 0.33, 0.66 };
  eig::Array2f vc = a * va + (1.f - a) * vb;

  auto pa = detail::pack_snorm_2x16(va);
  auto pb = detail::pack_snorm_2x16(vb);
  auto pc = std::bit_cast<uint>(a * std::bit_cast<float>(pa)
                      + (1.f - a) * std::bit_cast<float>(pb));

  eig::Array2f rc = detail::unpack_snorm_2x16(pc).unaryExpr([](float f) { return std::fmod(f, 1.f); });
  fmt::print("{} - {}\n", vc, rc); */
  
  /* // Load spectral basis
  auto basis = io::load_json("resources/misc/tree.json").get<BasisTreeNode>().basis;

  // Define base color system
  auto csys = ColrSystem {
    .cmfs       = models::cmfs_cie_xyz,
    .illuminant = models::emitter_cie_d65
  };

  // Define base colors
  Colr colr_a = { 0.5, 0.25, 0.1 };
  Colr colr_b = { 0.25, 0.75, 0.25};
  
  // Generate coefficients
  auto coef_a = generate_spectrum_coeffs(DirectSpectrumInfo {
    .direct_constraints = {{ csys, colr_a }},
    .basis              = basis
  }).array().eval();
  auto coef_b = generate_spectrum_coeffs(DirectSpectrumInfo {
    .direct_constraints = {{ csys, colr_b }},
    .basis              = basis
  }).array().eval();

  // Define valid interpolation output
  float alpha = .33f;
  Colr colr_c = alpha * colr_a + (1.f - alpha) * colr_b;

  // Placeholder additive means
  Spec mean_a = Spec(luminance(colr_a)).cwiseMin(1.f);
  Spec mean_b = Spec(luminance(colr_b)).cwiseMin(1.f);
  Spec mean_c = alpha * mean_a + (1.f - alpha) * mean_b;

  auto pack_a = detail::pack_snorm_12(coef_a);
  auto pack_b = detail::pack_snorm_12(coef_b);
  auto pack_c_= (alpha        * *reinterpret_cast<eig::Array4f*>(&pack_a) 
              + (1.f - alpha) * *reinterpret_cast<eig::Array4f*>(&pack_b)).eval();
  auto pack_c = *reinterpret_cast<eig::Array4u*>(&pack_c_);
  
  // Perform interpolation on coefficients or their packing
  auto coef_c = detail::unpack_snorm_12(pack_c);
  // auto coef_c = (alpha * coef_a + (1.f - alpha) * coef_b).eval();
  
  // Recover result from mixed coefficients
  auto spec_c = (basis(coef_c.matrix()) + mean_c).eval();
  auto rtrp_c = csys(spec_c);

  fmt::print("colr_c : {}\nrtrp_c : {}\n", colr_c, rtrp_c); */
}