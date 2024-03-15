#include <catch2/catch_test_macros.hpp>
#include <catch2/benchmark/catch_benchmark.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>
#include <metameric/core/math.hpp>
#include <metameric/core/spectrum.hpp>
#include <metameric/core/ranges.hpp>
#include <metameric/core/utility.hpp>
#include <array>

using uint = unsigned;

template <uint n>
constexpr auto binomial_row() {
  std::array<uint, n> row {};
  row[0] = row[n - 1] = 1;
  if constexpr (n > 1) {
    auto prev_row = binomial_row<n - 1>();
    for (uint k = 1; k < n - 1; ++k)
      row[k] = prev_row[k - 1] + prev_row[k];
  }
  return row;
}

struct BinomialCoeff {
  uint coeff;
  uint x_power;
  uint y_power;
};

template <uint n>
constexpr auto binomial_sum() {
  std::array<BinomialCoeff, n + 1> weights {};
  auto row = binomial_row<n + 1>();
  for (uint k = 0; k <= n; ++k)
    weights[k] = { .coeff = row[k], .x_power = k, .y_power = n - k };
  return weights;
}

template <uint n>
constexpr auto triangular_number() { return (n * (n + 1)) / 2; }

template <uint n>
constexpr auto triangle_sum() {
  std::array<BinomialCoeff, triangular_number<n + 1>()> weights {};
  if constexpr (n == 0) {
    weights[0] = { 1, 0, 0 };
  } else {
    met::rng::copy(triangle_sum<n - 1>(), weights.begin());
    met::rng::copy(binomial_sum<n>(),     weights.begin() + triangular_number<n>());
  }
  return weights;
}

constexpr auto bin_row = binomial_row<4>();
constexpr auto bin_sum = binomial_sum<2>();
constexpr auto tri_sum = triangle_sum<3>();

TEST_CASE("Matrix shenanigans") {
  using CMFS = met::CMFS::PlainMatrix;
  using Spec = met::Spec::PlainMatrix;
  using Colr = met::Colr::PlainMatrix;
  namespace rng = std::ranges;
  namespace vws = std::views;
  
  // Series settings
  constexpr uint n = 4;

  // Inputs
  CMFS A = CMFS::Ones(); // 64x3 matrix
  Spec x = 0.5f;         // 64x1 vector
  Spec y = -0.4f;        // 64x1 vector

  Colr c1;
  BENCHMARK("Baseline") {
    c1 = 0.f;
    for (uint i = 0; i < n; ++i)
      c1 += A.transpose() 
          * (x + y).array().pow(static_cast<float>(i)).matrix();
  };

  // Precompute powers
  constexpr auto binomial_chain = triangle_sum<n - 1>();
  auto y_muls = vws::iota(0u, n)
              | vws::transform([&y](uint i) { 
                  return y.array().pow(static_cast<float>(i)).matrix().eval(); })
              | rng::to<std::vector<Spec>>();
  auto y_divs = y_muls
              | vws::transform([](Spec &s) { return (1.f / s.array()).matrix().eval(); })
              | rng::to<std::vector<Spec>>();

  // Precomupte cy
  Colr cy = 0.f;
  for (auto coeffs : binomial_chain)
    cy += A.transpose()
          * (static_cast<float>(coeffs.coeff)
          * y_muls[coeffs.y_power].array()).matrix();

  Colr c2;
  BENCHMARK("Binomial") {
    c2 = 0.f;
    for (auto coeffs : binomial_chain)
      c2 += A.transpose() 
          * (static_cast<float>(coeffs.coeff)
          * x.array().pow(static_cast<float>(coeffs.x_power))
          * y_muls[coeffs.y_power].array()).matrix();
  };

  // Test equality
  CHECK(c1.isApprox(c2));
  fmt::print("OUTPUT: c1 = {}, c2 = {}\n", c1, c2);
}