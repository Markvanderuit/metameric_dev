#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>
#include <metameric/core/distribution.hpp>
#include <metameric/core/math.hpp>
#include <metameric/core/utility.hpp>

// Test in metameric namespace
using namespace met;

template <typename Ty>
void swap_ip(Ty &a, Ty &b) {
  std::swap(a, b);
}

TEST_CASE("Huh") {
  int w = 1024,  h = 767;
  float x = 3.14153, y = 2.17;
  
  swap_ip(w, h);
  swap_ip(x, y);

  fmt::print("w {}, h {}\n", w, h);
  fmt::print("x {}, y {}\n", x, y);
}
