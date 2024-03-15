#include <catch2/catch_test_macros.hpp>
#include <catch2/benchmark/catch_benchmark.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>
#include <metameric/core/math.hpp>
#include <metameric/core/ranges.hpp>
#include <metameric/core/spectrum.hpp>
#include <metameric/core/utility.hpp>

TEST_CASE("Matrix shenanigans") {
  using namespace met;

  CMFS cmfs = models::cmfs_cie_xyz;
  Spec illm = models::emitter_cie_e;
  
  CMFS to_xyz = (cmfs.array().colwise() * illm)
              / (cmfs.array().col(1) * illm).sum();
  CMFS to_rgb = (models::xyz_to_srgb_transform * to_xyz.transpose()).transpose();
  
  Colr c = to_rgb.transpose() * Spec(1).matrix();
  fmt::print("Value: {}, luminance: {}\n", c, luminance(c));
}