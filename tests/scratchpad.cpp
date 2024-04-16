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
#include <metameric/core/detail/packing.hpp>
#include <complex>

TEST_CASE("Spectrum shenanigans") {
  using namespace met;
  using namespace std::complex_literals;


  constexpr double y_inscpt = 3.0;
  constexpr double flt_j    = 2.0;
  constexpr double phase_next = -0.15;
  // constexpr auto common_summands = y_inscpt * (1.0if / flt_j); 
  // constexpr eig::scomplex v = { 0.f, y_inscpt / flt_j };

  auto vd = std::exp(-1.0i * flt_j * phase_next);
  auto vr = eig::dcomplex { std::cos(flt_j * phase_next), std::sin(flt_j * phase_next) };

  fmt::print("{}+{}j vs {}+{}j\n", vd.real(), vd.imag(), vr.real(), vr.imag());

  // vec2(0, - 1 * flt_j * phase_next)

  // vec2(0, 1) * vec2(v, 0)
  //       return vec2(0, v);
  // vec2(v, 0) * vec2(0, 1)
  //   return vec2(0, v);


// vec2 complex_mult(vec2 lhs, vec2 rhs) {
//   return vec2(lhs.x * rhs.x - lhs.y * rhs.y,
//               lhs.x * rhs.y + lhs.y * rhs.x);
// }
  
  // vec2 complex_exp(vec2 v) {
    // float e = exp(v.x);
    // return vec2(e * cos(v.y), e * sin(v.y));
  // }

  // return vec2(lhs.x * rhs.x - lhs.y * rhs.y,
  //             lhs.x * rhs.y + lhs.y * rhs.x);

  // return vec2(num.x * denom.x + num.y * denom.y,
  //            -num.x * denom.y + num.y * denom.x) / sdot(denom);
  // vec2(0, flt_j) / sdot()
  // vec2)
}