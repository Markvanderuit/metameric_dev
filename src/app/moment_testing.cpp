#include <metameric/core/math.hpp>
#include <metameric/core/moments.hpp>
#include <metameric/core/ranges.hpp>
#include <metameric/core/spectrum.hpp>
#include <metameric/core/utility.hpp>
#include <small_gl/buffer.hpp>
#include <small_gl/dispatch.hpp>
#include <small_gl/program.hpp>
#include <small_gl/window.hpp>
#include <small_gl/utility.hpp>
#include <fmt/core.h>
#include <fmt/ranges.h>
#include <vector>

int main() {
  using namespace met;

  Moments tm        = { 0.53361477, 0.03668047, -0.02211483, -0.04177091, -0.04679692,  0.01339208, 0.06915859,  0.02681544, 0.0, 0.0, 0.0, 0.0 };
  eig::Array4f wvls = { 0.125, 0.325, 0.55, 0.9 };
  
  // Make OpenGL context available
  gl::Window window = {{ .flags = gl::WindowFlags::eDebug }};
  gl::debug::enable_messages(gl::DebugMessageSeverity::eHigh, gl::DebugMessageTypeFlags::eAll);

  gl::Program program = {{ .type = gl::ShaderType::eCompute,
                           .spirv_path = "resources/shaders/test/test_moments.comp.spv",
                           .cross_path = "resources/shaders/test/test_moments.comp.json" }};

  gl::Buffer in_buffer  = {{ .data = cnt_span<const std::byte>(tm)   }};
  gl::Buffer wvl_buffer = {{ .data = cnt_span<const std::byte>(wvls) }};
  gl::Buffer out_buffer = {{ .size = sizeof(decltype(wvls))          }};


  program.bind();
  program.bind("b_in",  in_buffer);
  program.bind("b_wvl", wvl_buffer);
  program.bind("b_out", out_buffer);

  gl::dispatch_compute({ .groups_x = 1 });

  eig::Array4f refl_gpu;
  out_buffer.get_as<float>(cnt_span<float>(refl_gpu));

  auto refl_cpu = moments_to_reflectance(wvls, tm);

  fmt::print("Input        : {}\n", tm);
  fmt::print("Output (cpu) : {}\n", refl_cpu);
  fmt::print("Output (gpu) : {}\n", refl_gpu);
  
  return 0;
}