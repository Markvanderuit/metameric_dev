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

TEST_CASE("svd") {
  using namespace met;
  
  // Load spectral basis
  auto basis = io::load_basis("resources/misc/basis_262144.txt");
  basis.func.array().rowwise() /= basis.func.colwise().maxCoeff().cwiseMax(
                                  basis.func.colwise().minCoeff().cwiseAbs()).array().eval();;

  // Define base color system
  auto csys = ColrSystem {
    .cmfs       = models::cmfs_cie_xyz,
    .illuminant = models::emitter_cie_d65
  };

  // Define base colors
  std::vector<std::pair<std::string, Colr>> test_data = {
    { "random a", Colr { .25f, .75f, .25f }},
    { "random b", Colr { .33f, .33f, .33f }}
  };

  for (const auto &[name, colr] : test_data) {
    auto coef_input = generate_spectrum_coeffs(DirectSpectrumInfo {
      .direct_constraints = {{ csys, colr }},
      .basis              = basis
    }).array().eval();
    Spec spec_input = basis(coef_input);

    auto coef_outpt = generate_spectrum_coeffs(SpectrumCoeffsInfo {
      .spec  = spec_input,
      .basis = basis
    });
    Spec spec_outpt = basis(coef_outpt);
    
    auto colr_input = csys(spec_input);
    auto colr_outpt = csys(spec_outpt);

    auto spec_err = (spec_input - spec_outpt).matrix().norm();
    fmt::print("colr err: {} and {}\n", (colr_input - colr).matrix().norm(), 
                                        (colr_outpt - colr).matrix().norm());
    fmt::print("spec err: {}\n", spec_err);
  } // for (...)
}