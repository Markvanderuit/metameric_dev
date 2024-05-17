#include <catch2/catch_test_macros.hpp>
#include <metameric/core/detail/packing.hpp>
#include <metameric/core/constraints.hpp>
#include <metameric/core/matching.hpp>
#include <metameric/core/math.hpp>
#include <metameric/core/moments.hpp>
#include <metameric/core/ranges.hpp>
#include <metameric/core/distribution.hpp>
#include <metameric/core/moments.hpp>
#include <metameric/core/metamer.hpp>
#include <metameric/core/spectrum.hpp>
#include <metameric/core/json.hpp>
#include <metameric/core/utility.hpp>
#include <metameric/components/schedule.hpp>
#include <metameric/components/misc/task_frame_begin.hpp>
#include <metameric/components/misc/task_frame_end.hpp>
#include <metameric/components/views/task_window.hpp>
#include <metameric/components/views/detail/imgui.hpp>
#include <metameric/components/views/detail/task_viewport.hpp>
#include <metameric/components/views/detail/task_arcball_input.hpp>
#include <small_gl/window.hpp>
#include <small_gl/program.hpp>
#include <small_gl/buffer.hpp>
#include <small_gl/array.hpp>
#include <small_gl/dispatch.hpp>
#include <small_gl/utility.hpp>
#include <oneapi/tbb/concurrent_vector.h>
#include <algorithm>
#include <execution>
#include <sstream>

using namespace met;

TEST_CASE("single_spectrum_generation") {
  Spec input = { 0.13300,0.13479,0.13489,0.13384,0.13281,0.13239,0.13193,0.13104,0.13003,0.12913,0.12818,0.12703,0.12555,0.12352,0.12106,0.11864,0.11632,0.11398,0.11164,0.10930,0.10695,0.10461,0.10231,0.09992,0.09737,0.09517,0.09386,0.09316,0.09251,0.09205,0.09210,0.09268,0.09341,0.09482,0.09690,0.10105,0.11101,0.12889,0.15980,0.20591,0.26298,0.32627,0.38925,0.44390,0.48851,0.52315,0.54782,0.56461,0.57490,0.58155,0.58600,0.58864,0.59013,0.59116,0.59222,0.59302,0.59333,0.59389,0.59543,0.59754,0.59941,0.60122,0.60351,0.60601 };
  
  // Moment roundtrip
  {
    // Generate moments, then go back to spectrum
    auto moments = spectrum_to_moments(input);
    auto output  = moments_to_spectrum_lagrange(moments);

     fmt::print("moments: {}\n", output);
  }

  // Basis roundtrip
  {
    // Load spectral basis
    // Normalize if they not already normalized
    auto basis = io::load_basis("resources/misc/basis_262144.txt");
    for (auto col : basis.func.colwise()) {
      auto min_coeff = col.minCoeff(), max_coeff = col.maxCoeff();
      
      col /= std::max(std::abs(max_coeff), std::abs(min_coeff));
    }

    // Generate coefficients, then go back to spectrum
    auto coeffs = generate_spectrum_coeffs(SpectrumCoeffsInfo {
      .spec    = input,
      .basis   = basis
    });
    auto output = basis(coeffs);

    fmt::print("basis: {}\n", output);
  }
}