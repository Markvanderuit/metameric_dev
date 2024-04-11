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

  Spec spectrum_a = models::cmfs_cie_xyz.col(0);
  Spec spectrum_b = models::cmfs_cie_xyz.col(2);
  spectrum_a = spectrum_a / spectrum_a.maxCoeff() * 0.6f;
  spectrum_b = spectrum_b / spectrum_b.maxCoeff() * 0.35f;

  Moments moments_a = spectrum_to_moments(spectrum_a);
  Moments moments_b = spectrum_to_moments(spectrum_b);

  /* { // Test unpacked, cpu-side interpolation
    float a = 0.5f;
    Spec spectrum_c  = (1.f - a) * spectrum_a + a * spectrum_b;
    // Spec spectrum_c  = moments_to_spectrum(spectrum_to_moments((1.f - a) * spectrum_a + a * spectrum_b));
    Spec spectrum_c2 = moments_to_spectrum((1.f - a) * moments_a  + a * moments_b);

    fmt::print("Direct   {}\n", spectrum_c);
    fmt::print("Moments: {}\n", spectrum_c2);
    fmt::print("Error:   {}\n", (spectrum_c - spectrum_c2).abs().eval());
    fmt::print("Maximum: {}\n", (spectrum_c - spectrum_c2).abs().maxCoeff());
      
    std::exit(0);
  } */

  { // Test packed interpolation. Yikes
    eig::Array4u packing_a = pack_moments_12x10(moments_a);
    eig::Array4u packing_b = pack_moments_12x10(moments_b);

    constexpr float a = .5f;
    constexpr uint ax = (uint) (a         * (4294967296u));
    constexpr uint ay = (uint) ((1.f - a) * (4294967296u));

    Moments moments_c =  moments_a; // (1.f - a) * moments_a + a * moments_b;
    Moments moments_c2 = unpack_moments_12x10(packing_a); // ax * packing_a + ay * packing_b);
    // Moments moments_c2 = unpack_moments_12x10(((1.f - a) * packing_a.cast<float>()).cast<uint>() 
    //                    + (a * packing_b.cast<float>()).cast<uint>());

    fmt::print("Direct   {}\n", moments_c);
    fmt::print("Mixed:   {}\n", moments_c2);
    fmt::print("Error:   {}\n", (moments_c - moments_c2).abs().eval());
    fmt::print("Maximum: {}\n", (moments_c - moments_c2).abs().maxCoeff());
      
    std::exit(0);
  }

  /* Moments tm        = { 0.53361477, 0.03668047, -0.02211483, -0.04177091, -0.04679692,  0.01339208, 0.06915859,  0.02681544, 0.0, 0.0, 0.0, 0.0 };
  eig::Array4f wvls = { 0.125, 0.325, 0.55, 0.9 };
  eig::Array4i pack = pack_moments_12x10(tm);
  
  // Make OpenGL context available
  gl::Window window = {{ .flags = gl::WindowFlags::eDebug }};
  gl::debug::enable_messages(gl::DebugMessageSeverity::eHigh, gl::DebugMessageTypeFlags::eAll);

  gl::Program program = {{ .type = gl::ShaderType::eCompute,
                           .spirv_path = "resources/shaders/test/test_moments.comp.spv",
                           .cross_path = "resources/shaders/test/test_moments.comp.json" }};

  gl::Buffer in_buffer  = {{ .data = cnt_span<const std::byte>(pack) }};
  gl::Buffer wvl_buffer = {{ .data = cnt_span<const std::byte>(wvls) }};
  gl::Buffer out_buffer = {{ .size = sizeof(decltype(wvls))          }};

  program.bind();
  program.bind("b_in",  in_buffer);
  program.bind("b_wvl", wvl_buffer);
  program.bind("b_out", out_buffer);

  gl::dispatch_compute({ .groups_x = 1 });


  eig::Array4f refl_cpu = moments_to_reflectance(wvls, tm);
  eig::Array4f refl_gpu;
  out_buffer.get_as<float>(cnt_span<float>(refl_gpu));

  fmt::print("Input        : {}\n", tm);
  fmt::print("Output (cpu) : {}\n", refl_cpu);
  fmt::print("Output (gpu) : {}\n", refl_gpu);
  fmt::print("Error        : {}\n", (refl_cpu - refl_gpu).abs().eval()); */
  
  return 0;
}