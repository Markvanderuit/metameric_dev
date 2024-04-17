#include <catch2/catch_test_macros.hpp>
#include <metameric/core/constraints.hpp>
#include <metameric/core/matching.hpp>
#include <metameric/core/math.hpp>
#include <metameric/core/moments.hpp>
#include <metameric/core/ranges.hpp>
#include <metameric/core/spectrum.hpp>
#include <metameric/core/utility.hpp>
#include <small_gl/window.hpp>
#include <small_gl/program.hpp>
#include <small_gl/buffer.hpp>
#include <small_gl/dispatch.hpp>
#include <small_gl/utility.hpp>
#include <fmt/core.h>
#include <fmt/ranges.h>
#include <complex>
#include <numbers>
#include <string_view>

TEST_CASE("Moment gl-side rewrite") {
  using namespace met;

  // Make OpenGL context available
  gl::Window window = {{ .flags = gl::WindowFlags::eDebug }};
  gl::debug::enable_messages(gl::DebugMessageSeverity::eHigh, gl::DebugMessageTypeFlags::eAll);

  gl::Program program = {{ .type = gl::ShaderType::eCompute,
                           .spirv_path = "test/test_moments.comp",
                           .cross_path = "test/test_moments.json" }};

  auto sign_data = Spec(0.5f * models::emitter_cie_d65 / models::emitter_cie_d65.maxCoeff()) ;
  auto warp_data = generate_warped_phase();
  gl::Buffer buffer_warp = {{ .data = cnt_span<const std::byte>(warp_data) }};
  gl::Buffer buffer_sign = {{ .data = cnt_span<const std::byte>(sign_data) }};
  gl::Buffer buffer_out  = {{ .size = sizeof(eig::Array4u) }};

  program.bind();
  program.bind(std::string_view("b_phase"),  buffer_warp);
  program.bind(std::string_view("b_signal"), buffer_sign);
  program.bind(std::string_view("b_output"), buffer_out);

  gl::dispatch_compute({ .groups_x = 1 });

  eig::Array4u pack = 0;
  buffer_out.get_as<uint>(std::span { pack.data(), 4 });

  auto moments = unpack_moments_12x10(pack);
  fmt::print("Moments: {}\n", moments);
}