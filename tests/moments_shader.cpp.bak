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

  Moments tm              = { 0.53361477, 0.03668047, -0.02211483, -0.04177091, -0.04679692,  0.01339208, 0.06915859,  0.02681544, 0.0, 0.0, 0.0, 0.0 };
  std::vector<float> wvls = { 0.1, 0.3, 0.6, 0.8 };
  
  // Make OpenGL context available
  gl::Window window = {{ .flags = gl::WindowFlags::eDebug }};
  gl::debug::enable_messages(gl::DebugMessageSeverity::eHigh, gl::DebugMessageTypeFlags::eAll);

  gl::Program program = {{ .type = gl::ShaderType::eCompute,
                           .spirv_path = "test/test_moments.comp",
                           .cross_path = "test/test_moments.json",
                           .spec_const = {{ 0, static_cast<uint>(wvls.size()) }} }};

  gl::Buffer in_buffer  = {{ .data = cnt_span<const std::byte>(tm)   }};
  gl::Buffer wvl_buffer = {{ .data = cnt_span<const std::byte>(wvls) }};
  gl::Buffer out_buffer = {{ .size = wvls.size() * sizeof(float)     }};

  program.bind();
  program.bind(std::string_view("b_in"),  in_buffer);
  program.bind(std::string_view("b_wvl"), wvl_buffer);
  program.bind(std::string_view("b_out"), out_buffer);

  gl::dispatch_compute({ .groups_x = 1 });

  std::vector<float> out(wvls.size());
  out_buffer.get_as<float>(out);

  fmt::print("{}\n", out);
}