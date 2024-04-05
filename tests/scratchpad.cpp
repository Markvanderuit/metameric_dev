#include <catch2/catch_test_macros.hpp>
#include <catch2/benchmark/catch_benchmark.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>
#include <metameric/core/math.hpp>
#include <metameric/core/ranges.hpp>
#include <metameric/core/matching.hpp>
#include <metameric/core/spectrum.hpp>
#include <metameric/core/constraints.hpp>
#include <metameric/core/components.hpp>
#include <metameric/core/moments.hpp>
#include <metameric/core/utility.hpp>
#include <complex>

TEST_CASE("Spectrum shenanigans") {
  using namespace met;

  /* {
    using namespace std::complex_literals;
    eig::Array<uint, 8, 1> v = { 0, 1, 2, 3, 4, 5, 6, 7 };
    uint j = 5;

    fmt::print("{}\n", v(eig::seq(j, 1, eig::fix<-1>))); // [j:0:-1]
    // fmt::print("{}\n", v(eig::seq(0, j - 1)));           // [0:j]
    // fmt::print("{}\n", v.head(j));                       // [0:j]
    // fmt::print("{}\n", v.head(j + 1).reverse());         // [j::-1]

    // fmt::print("{}\n", v.head(j + 1));         // [0:j+1]

    std::exit(0);
  } */

  Spec spec_gt = (models::emitter_cie_d65
                / models::emitter_cie_d65.maxCoeff()) * 0.75f;
  
  // Spec spec_gt = 0.5f;

  auto moment = spectrum_to_moments(spec_gt);
  auto spec_m = moments_to_spectrum(moment);

  fmt::print("---\n");
  fmt::print("Ground {}\n", spec_gt);
  fmt::print("Moment {}\n", moment);
  fmt::print("Rntrip {}\n", spec_m);
}