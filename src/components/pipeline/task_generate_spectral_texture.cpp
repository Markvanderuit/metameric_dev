#include <metameric/components/tasks/task_generate_spectral_texture.hpp>
#include <metameric/core/knn.hpp>
#include <metameric/core/spectrum.hpp>
#include <metameric/core/state.hpp>
#include <metameric/core/texture.hpp>
#include <small_gl/utility.hpp>
#include <ranges>

namespace met {
  GenerateSpectralTextureTask::GenerateSpectralTextureTask(const std::string &name)
  : detail::AbstractTask(name) { }

  void GenerateSpectralTextureTask::init(detail::TaskInitInfo &info) {
    // Get externally shared resources
    auto &e_rgb_texture = info.get_resource<ApplicationData>(global_key, "app_data").rgb_texture;

    const uint generate_n    = e_rgb_texture.size().prod();
    const uint generate_ndiv = ceil_div(generate_n, 256u);

    // Initialize objects for shader call
    m_generate_program = {{ .type = gl::ShaderType::eCompute,
                            .path = "resources/shaders/generate_spectral_task/generate_spectral.comp" }};
    m_generate_dispatch = { .groups_x = generate_ndiv, .bindable_program = &m_generate_program };

    // Set these uniforms once
    m_generate_program.uniform<uint>("u_n", generate_n);

    // Initialize main color texture buffer
    auto rgb_texture_al = io::as_aligned((e_rgb_texture));
    info.emplace_resource<gl::Buffer>("color_texture_buffer", {
      .data = cast_span<const std::byte>(rgb_texture_al.data())
    });

    // Initialize main spectral texture buffer
    info.emplace_resource<gl::Buffer>("spectral_texture_buffer", {
      .size = sizeof(Spec) * generate_n
    });
  }

  void GenerateSpectralTextureTask::eval(detail::TaskEvalInfo &info) {
    // Get shared resources
    auto &e_spectral_gamut_buffer   = info.get_resource<gl::Buffer>("generate_gamut", "spectral_gamut_buffer");
    auto &e_color_gamut_buffer      = info.get_resource<gl::Buffer>("generate_gamut", "color_gamut_buffer");
    auto &i_color_texture_buffer    = info.get_resource<gl::Buffer>("color_texture_buffer");
    auto &i_spectral_texture_buffer = info.get_resource<gl::Buffer>("spectral_texture_buffer");

    // Dispatch generate shader
    e_color_gamut_buffer.bind_to(gl::BufferTargetType::eShaderStorage,      0);
    e_spectral_gamut_buffer.bind_to(gl::BufferTargetType::eShaderStorage,   1);
    i_color_texture_buffer.bind_to(gl::BufferTargetType::eShaderStorage,    2);
    i_spectral_texture_buffer.bind_to(gl::BufferTargetType::eShaderStorage, 3);
    gl::sync::memory_barrier(gl::BarrierFlags::eShaderStorageBuffer);
    gl::dispatch_compute(m_generate_dispatch);
  }
} // namespace met