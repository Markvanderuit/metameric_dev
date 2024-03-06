#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>
#include <metameric/core/math.hpp>
#include <metameric/core/spectrum.hpp>
#include <metameric/core/ranges.hpp>
#include <metameric/core/utility.hpp>

using namespace met;

TEST_CASE("Matrix powers") {
  // Base types 
  using type_a = Basis::BMat::PlainMatrix; // 64 x 12
  using type_x = Basis::BVec::PlainMatrix; // 12 x 1
  using type_d = eig::Matrix<float, wavelength_samples, wavelength_samples>;

  SECTION("Confirm transpose") {
    auto A = type_a::Random().eval();
    auto x = type_x::Random().eval();

    auto v1 = (A * x).eval();
    auto v2 = (x.transpose() * A.transpose()).transpose().eval();

    CHECK(v1.isApprox(v2));
  }

  SECTION("Confirm associative") {
    auto A = type_a::Random().eval();
    auto x = type_x::Random().eval();
    
    type_d diag = (A * x).asDiagonal();

    auto v1 = (A * x).array().pow(3.f).matrix().eval();
    auto v2 = (diag * diag * diag).diagonal().eval();

    // auto v2 = ((A * A.transpose().eval()) * x.cwiseProduct(x)).eval();

    // fmt::print("v1 = {}\n", v1);
    // fmt::print("v2 = {}\n", v2);

    CHECK(v1.isApprox(v2));
  }
}