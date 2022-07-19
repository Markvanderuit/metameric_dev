#include <metameric/components/tasks/generate_spectral_task.hpp>
#include <metameric/core/io.hpp>
#include <metameric/core/knn.hpp>
#include <metameric/core/spectrum.hpp>
#include <small_gl/utility.hpp>
#include <ranges>

namespace met {
  GenerateSpectralTask::GenerateSpectralTask(const std::string &name)
  : detail::AbstractTask(name) { }

  void GenerateSpectralTask::init(detail::TaskInitInfo &info) {
    // Get externally shared resources
    auto &e_texture_obj = info.get_resource<io::TextureData<Color>>("global", "color_texture_buffer_cpu");

    const uint generate_n    = glm::prod(e_texture_obj.size);
    const uint generate_ndiv = ceil_div(generate_n, 256u);

    // Initialize objects for shader call
    m_generate_program = {{ .type = gl::ShaderType::eCompute,
                            .path = "resources/shaders/generate_spectral_task/generate_spectral.comp" }};
    m_generate_dispatch = { .groups_x = generate_ndiv, .bindable_program = &m_generate_program };

    // Set these uniforms once
    m_generate_program.uniform<uint>("u_n", generate_n);

    // Initialize spectral gamut buffer
    info.emplace_resource<gl::Buffer, gl::BufferCreateInfo>("spectral_gamut_buffer", {
      .size  = sizeof(Spec) * 4, 
      .flags = gl::BufferCreateFlags::eMapRead | gl::BufferCreateFlags::eMapWrite
    });

    // Initialize spectral texture buffer
    info.emplace_resource<gl::Buffer, gl::BufferCreateInfo>("spectral_texture_buffer", {
      .size = sizeof(Spec) * generate_n
    });
  }

  void GenerateSpectralTask::eval(detail::TaskEvalInfo &info) {
    // Get shared resources
    auto &i_spectral_texture_buffer = info.get_resource<gl::Buffer>("spectral_texture_buffer");
    auto &i_spectral_gamut_buffer   = info.get_resource<gl::Buffer>("spectral_gamut_buffer");
    auto &e_color_texture_buffer    = info.get_resource<gl::Buffer>("global", "color_texture_buffer_gpu");
    auto &e_color_gamut_buffer      = info.get_resource<gl::Buffer>("global", "color_gamut_buffer");
    auto &e_spectral_knn_grid       = info.get_resource<KNNGrid<Spec>>("global", "spectral_knn_grid");

    // Open temporary mappings to color/spectral gamut buffer 
    auto color_gamut_map    = convert_span<eig::AlArray3f>(e_color_gamut_buffer.map(gl::BufferAccessFlags::eMapReadWrite));
    auto spectral_gamut_map = convert_span<Spec>(i_spectral_gamut_buffer.map(gl::BufferAccessFlags::eMapReadWrite));

    // Sample spectra at gamut positions from KNN object
    std::ranges::transform(color_gamut_map, spectral_gamut_map.begin(),
      [&](const auto &p) { return e_spectral_knn_grid.query_1_nearest(p).value; });

    // Close temporary mappings
    i_spectral_gamut_buffer.unmap();
    e_color_gamut_buffer.unmap();

    // Dispatch generate shader
    e_color_gamut_buffer.bind_to(gl::BufferTargetType::eShaderStorage,      0);
    i_spectral_gamut_buffer.bind_to(gl::BufferTargetType::eShaderStorage,   1);
    e_color_texture_buffer.bind_to(gl::BufferTargetType::eShaderStorage,    2);
    i_spectral_texture_buffer.bind_to(gl::BufferTargetType::eShaderStorage, 3);
    gl::sync::memory_barrier(gl::BarrierFlags::eShaderStorageBuffer);
    gl::dispatch_compute(m_generate_dispatch);
  }
} // namespace met