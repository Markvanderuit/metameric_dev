#include <metameric/components/tasks/task_gen_spectral_texture.hpp>
#include <metameric/core/knn.hpp>
#include <metameric/core/spectrum.hpp>
#include <metameric/core/state.hpp>
#include <metameric/core/texture.hpp>
#include <small_gl/utility.hpp>
#include <ranges>

namespace met {
  GenSpectralTextureTask::GenSpectralTextureTask(const std::string &name)
  : detail::AbstractTask(name) { }

  void GenSpectralTextureTask::init(detail::TaskInitInfo &info) {
    // Get externally shared resources
    auto &e_rgb_texture = info.get_resource<ApplicationData>(global_key, "app_data").loaded_texture;

    const uint generate_n    = e_rgb_texture.size().prod();
    const uint generate_ndiv = ceil_div(generate_n, 256u);

    // Initialize objects for shader call
    m_generate_program = {{ .type = gl::ShaderType::eCompute,
                            .path = "resources/shaders/generate_spectral_task/generate_spectral.comp" }};
    m_generate_dispatch = { .groups_x = generate_ndiv, .bindable_program = &m_generate_program };

    // Set these uniforms once
    m_generate_program.uniform("u_n", generate_n);

    // Initialize main color texture buffer
    // auto rgb_texture_al = io::as_aligned((e_rgb_texture));
    info.emplace_resource<gl::Buffer>("color_buffer", {
      .data = cast_span<const std::byte>(io::as_aligned((e_rgb_texture)).data())
    });

    // Initialize main spectral texture buffer
    info.emplace_resource<gl::Buffer>("spectrum_buffer", {
      .size  = sizeof(Spec) * generate_n,
      .flags = gl::BufferCreateFlags::eMapRead 
    });
  }

  void GenSpectralTextureTask::eval(detail::TaskEvalInfo &info) {
    // Get shared resources
    auto &e_color_gamut   = info.get_resource<gl::Buffer>("gen_spectral_gamut", "color_buffer");
    auto &e_spect_gamut   = info.get_resource<gl::Buffer>("gen_spectral_gamut", "spectrum_buffer");
    auto &i_color_texture = info.get_resource<gl::Buffer>("color_buffer");
    auto &i_spect_texture = info.get_resource<gl::Buffer>("spectrum_buffer");

    // Bind resources to buffer targets
    e_color_gamut.bind_to(gl::BufferTargetType::eShaderStorage,   0);
    e_spect_gamut.bind_to(gl::BufferTargetType::eShaderStorage,   1);
    i_color_texture.bind_to(gl::BufferTargetType::eShaderStorage, 2);
    i_spect_texture.bind_to(gl::BufferTargetType::eShaderStorage, 3);

    // Dispatch shader to generate spectral data
    gl::sync::memory_barrier(gl::BarrierFlags::eShaderStorageBuffer);
    gl::dispatch_compute(m_generate_dispatch);
  }
} // namespace met