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
  
  // Load spectral basis
  auto basis = io::load_basis("resources/misc/basis_262144.txt");
  
  // Normalize bases if they are not already normalized
  for (uint i = 0; i < basis.func.cols(); ++i) {
    auto col = basis.func.col(i).array().eval();
    auto min_coeff = col.minCoeff();
    auto max_coeff = col.maxCoeff();
    basis.func.col(i) = (col / std::max(std::abs(max_coeff), std::abs(min_coeff)));
    // basis.func.col(i) = (col - min_coeff) / (max_coeff - min_coeff);
  }

  // Define base color system
  auto csys = ColrSystem {
    .cmfs       = models::cmfs_cie_xyz,
    .illuminant = models::emitter_cie_d65
  };

  // Define base colors
  std::vector<std::pair<std::string, Colr>> test_data = {
    { "red",    Colr { 1.f,  0.f,  0.f  }},
    { "green",  Colr { 0.f,  1.f,  0.f  }},
    { "blue",   Colr { 0.f,  0.f,  1.f  }},
    { "black",  Colr { 0.f,  0.f,  0.f  }},
    { "white",  Colr { 1.f,  1.f,  1.f  }},
    { "random", Colr { .25f, .75f, .25f }},
    { "random", Colr { .33f, .33f, .33f }},
  };

  for (const auto &[name, colr] : test_data) {
    auto coef = generate_spectrum_coeffs(DirectSpectrumInfo {
      .direct_constraints = {{ csys, colr }},
      .basis              = basis
    }).array().eval();
    Spec uncl = basis(coef);
    Spec spec = uncl.cwiseMax(0.f).cwiseMin(1.f).eval();
    Colr rtrp = csys(spec);

    Colr err      = (colr - rtrp).abs();
    auto err_pass = (err <= 1e-3).all();
    auto bnd_pass = (spec >= 0.f && spec <= 1.f).all();

    fmt::print("{}:"
               "\n\tcoeffs: {}"
               "\n\tinput:  {}"
               "\n\toutput: {}"
               "\n\terror:  {} ({})"
               "\n\tbounds: {} (unclamped {} <= x <= {}) -> (clamped {} <= x <= {})\n", 
      name,
      coef.head<6>(), colr, rtrp, 
      err_pass ? "pass" : "fail", err, 
      bnd_pass ? "pass" : "fail", 
      uncl.minCoeff(), uncl.maxCoeff(),
      spec.minCoeff(), spec.maxCoeff());
  } // for (...)
}