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
  
  Uplifting::Vertex v;
  v.constraint = DirectSurfaceConstraint { };

  v.constraint | visit {
    [](auto &v) requires(is_surface_constraint<std::decay_t<decltype(v)>>) {
    // [](is_surface_constraint auto &v) {
      using Ty = std::decay_t<decltype(v)>;
      fmt::print("Visited surface constraint for {}\n", typeid(Ty).name());
    },
    [](auto &v) {
      using Ty = std::decay_t<decltype(v)>;
      fmt::print("Visited remainder constraint for {}\n", typeid(Ty).name());
    }
  };

  std::visit(visit {
    [](is_surface_constraint auto &v) {
      using Ty = std::decay_t<decltype(v)>;
      fmt::print("Visited surface constraint for {}\n", typeid(Ty).name());
    },
    [](auto &v) {
      using Ty = std::decay_t<decltype(v)>;
      fmt::print("Visited remainder constraint for {}\n", typeid(Ty).name());
    }
  }, v.constraint);
  
  v.constraint | visit {
    [](is_surface_constraint auto &v) {
      using Ty = std::decay_t<decltype(v)>;
      fmt::print("Visited surface constraint for {}\n", typeid(Ty).name());
    },
    [](auto &v) {
      using Ty = std::decay_t<decltype(v)>;
      fmt::print("Visited remainder constraint for {}\n", typeid(Ty).name());
    }
  };
}