#include <catch2/catch_test_macros.hpp>
#include <catch2/benchmark/catch_benchmark.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>
#include <metameric/core/math.hpp>
#include <metameric/core/ranges.hpp>
#include <metameric/core/matching.hpp>
#include <metameric/core/spectrum.hpp>
#include <metameric/core/utility.hpp>

TEST_CASE("Matrix shenanigans") {
  using namespace met;

  std::variant<int, float, char> v = 0.5f;

  std::optional<int> o;
  
  o | match {
    [](const int &v) { fmt::print("optional holds {}\n", v); },
    []()             { fmt::print("optional holds none\n"); }
  };

  v | match {
    [](const int   &i) { fmt::print("int   {}\n", i); },
    [](const float &f) { fmt::print("float {}\n", f); },
    [](const char  &c) { fmt::print("char  {}\n", c); },
    [](const auto  &)  { fmt::print("unknown\n");     }
  };

  v | match_type([](auto vt, bool is_match) {
      fmt::print("holds {}: {}\n", typeid(decltype(vt)).name(), is_match);
  });
  
  v | match_one([](const uint &v) {
    fmt::print("matched one: {}\n", v);
  });
}