#include <metameric/core/math.hpp>
#include <metameric/core/moments.hpp>
#include <metameric/core/ranges.hpp>
#include <metameric/core/spectrum.hpp>
#include <metameric/core/utility.hpp>
#include <metameric/core/metamer.hpp>
#include <metameric/core/json.hpp>
#include <metameric/core/io.hpp>
#include <metameric/core/tree.hpp>
#include <small_gl/buffer.hpp>
#include <small_gl/dispatch.hpp>
#include <small_gl/program.hpp>
#include <small_gl/window.hpp>
#include <small_gl/utility.hpp>
#include <fmt/core.h>
#include <fmt/ranges.h>
#include <nlohmann/json.hpp>
#include <vector>

int main() {
  using namespace met;

  // Make OpenGL context available
  gl::Window window = {{ .flags = gl::WindowFlags::eDebug }};
  gl::debug::enable_messages(gl::DebugMessageSeverity::eHigh, gl::DebugMessageTypeFlags::eAll);
  
  // Setup shader program
  gl::Program program = {{ .type = gl::ShaderType::eCompute,
                           .spirv_path = "resources/shaders/test/test_moments.comp.spv",
                           .cross_path = "resources/shaders/test/test_moments.comp.json" }};

  // Load spectral basis
  auto basis = io::load_json("resources/misc/tree.json").get<BasisTreeNode>().basis;

  // Load test color system
  auto csys = ColrSystem {
    .cmfs       = models::cmfs_cie_xyz,
    .illuminant = models::emitter_cie_d65
  };

  // Setup test colors, matching spectra, and phase for generating moment coefficients
  auto test_colrs = { Colr(0.5),
                      Colr(0.75, 0.25, 0.25),
                      Colr(0.25, 0.75, 0.25),
                      Colr(0.05, 0.05, 0.95),
                      Colr(0.67, 0.33, 0.5) };
  auto test_specs = test_colrs
                  | vws::transform([&](const Colr &c) {
                    return generate_spectrum(DirectSpectrumInfo {
                      .direct_constraints = {{ csys, c }},
                      .basis              = basis
                    });
                  })
                  | rng::to<std::vector>();
  auto test_phase = generate_warped_phase();

  // Setup input/output buffers
  gl::Buffer buffer_phase  = {{ .data = cnt_span<const std::byte>(test_phase) }};
  gl::Buffer buffer_signal = {{ .size = sizeof(Spec),         .flags = gl::BufferCreateFlags::eStorageDynamic }};
  gl::Buffer buffer_output = {{ .size = sizeof(eig::Array4u), .flags = gl::BufferCreateFlags::eStorageDynamic }};

  // Prepare program state
  program.bind();
  program.bind(std::string_view("b_phase"),  buffer_phase);
  program.bind(std::string_view("b_signal"), buffer_signal);
  program.bind(std::string_view("b_output"), buffer_output);

  // Iterate colors and matching spectra
  for (const auto &[colr, spec] : vws::zip(test_colrs, test_specs)) {
    // Copy spectrum as signal, and clear output
    buffer_signal.set(cnt_span<const std::byte>(spec));
    buffer_output.clear();
    
    // Run moment computation gpu-side
    gl::dispatch_compute({ .groups_x = 1 });

    // Recover packed moment coefficients
    eig::Array4u data;
    buffer_output.get(cnt_span<std::byte>(data));

    // Recover moment coefficients on both sides for comparison
    auto moments_cpu  = detail::unpack_half_8x16(detail::pack_half_8x16(spectrum_to_moments(spec)));
    auto moments_gpu  = detail::unpack_half_8x16(data);

    // Recover color signals from reconstructed spectra
    auto colr_base = csys(spec); // Roundtrip should be exact
    auto colr_cpu  = csys(moments_to_spectrum(moments_cpu));
    auto colr_gpu  = csys(moments_to_spectrum(moments_gpu));

    fmt::print("For {},\n\tcpu = {} (error {})\n\tgpu = {} (error {})\n", 
      colr_base, 
      colr_cpu, (colr_base - colr_cpu).matrix().norm(), 
      colr_gpu, (colr_base - colr_gpu).matrix().norm());
  }

  // // auto sign_data = Spec(0.5);
  // auto sign_data = Spec(0.5f * models::emitter_cie_d65 / models::emitter_cie_d65.maxCoeff()) ;
  // gl::Buffer buffer_warp = {{ .data = cnt_span<const std::byte>(test_phase) }};
  // gl::Buffer buffer_sign = {{ .data = cnt_span<const std::byte>(sign_data) }};
  // gl::Buffer buffer_out  = {{ .size = sizeof(eig::Array4u) }};


  // program.bind();
  // program.bind(std::string_view("b_phase"),  buffer_warp);
  // program.bind(std::string_view("b_signal"), buffer_sign);
  // program.bind(std::string_view("b_output"), buffer_out);

  // gl::dispatch_compute({ .groups_x = 1 });

  // eig::Array4u pack = 0;
  // buffer_out.get_as<uint>(std::span { pack.data(), 4 });
  

  // auto colr_base = csys(sign_data);
  
  // auto moments_cpu  = /* unpack_moments_12x10(pack_moments_12x10( */spectrum_to_moments(sign_data)/* )) */;
  // auto moments_gpu  = unpack_moments_12x10(pack);
  // auto spectrum_cpu = moments_to_spectrum(moments_cpu);
  // auto spectrum_gpu = moments_to_spectrum(moments_gpu);
  // auto error_cpu    = (colr_base - csys(spectrum_cpu)).eval();
  // auto error_gpu    = (colr_base - csys(spectrum_gpu)).eval();

  // auto error = (moments_gpu - moments_cpu).abs().eval();
  // fmt::print("cpu : {} (error: {})\n", moments_cpu, error_cpu);
  // fmt::print("gpu : {} (error: {})\n", moments_gpu, error_gpu);
  // fmt::print("err : {}\n", error);

  return 0;
}