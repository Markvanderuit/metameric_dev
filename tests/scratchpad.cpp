#include <catch2/catch_test_macros.hpp>
#include <catch2/benchmark/catch_benchmark.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>
#include <metameric/core/math.hpp>
#include <metameric/core/ranges.hpp>
#include <metameric/core/matching.hpp>
#include <metameric/core/spectrum.hpp>
#include <metameric/core/constraints.hpp>
#include <metameric/core/components.hpp>
#include <metameric/core/utility.hpp>

TEST_CASE("Matrix shenanigans") {
  using namespace met;
  
  std::variant<Colr,  uint> diffuse;

  diffuse = Colr(0.5f);

  diffuse | visit {
    [&](const uint &i) { fmt::print("Integer\n"); },
    [](const Colr &c) { fmt::print("Color\n"); }
  };
}