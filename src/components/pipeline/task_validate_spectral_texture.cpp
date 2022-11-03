#include <metameric/components/pipeline/task_validate_spectral_texture.hpp>
#include <metameric/core/math.hpp>
#include <metameric/core/spectrum.hpp>
#include <metameric/core/state.hpp>
#include <metameric/core/detail/trace.hpp>
#include <small_gl/buffer.hpp>
#include <small_gl/utility.hpp>

namespace met {
  ValidateSpectralTextureTask::ValidateSpectralTextureTask(const std::string &name)
  : detail::AbstractTask(name) { }

  void ValidateSpectralTextureTask::init(detail::TaskInitInfo &info) {
    met_trace_full();

    // Get shared resources
    auto &e_rgb_texture = info.get_resource<ApplicationData>(global_key, "app_data").loaded_texture;

    // Determine dispatch size for clustered shader call
    const uint generate_n    = e_rgb_texture.size().prod();
    const uint generate_ndiv = ceil_div(generate_n, 256u / ceil_div(wavelength_samples, 4u));

    // Initialize objects for clustered shader call
    m_program = {{ .type = gl::ShaderType::eCompute,
                   .path = "resources/shaders/validate_spectral_texture/validate_spectral_texture_cl.comp.spv_opt",
                   .is_spirv_binary = true }};
    m_dispatch = { .groups_x = generate_ndiv, 
                   .bindable_program = &m_program }; 

    // Initialize validation result buffer
    info.emplace_resource<gl::Buffer>("validation_buffer", { .size  = sizeof(bool) * generate_n });
  }

  void ValidateSpectralTextureTask::eval(detail::TaskEvalInfo &info) {
    met_trace_full();

    // Validate spectral texture only on relevant state change
    auto &e_state_spec = info.get_resource<std::array<CacheState, 4>>("project_state", "gamut_spec");
    guard(std::ranges::any_of(e_state_spec, [](auto s) { return s == CacheState::eStale; }));

    // Get shared resources
    auto &e_spectral_texture  = info.get_resource<gl::Buffer>("gen_spectral_texture", "spectrum_buffer");
    auto &i_validation_buffer = info.get_resource<gl::Buffer>("validation_buffer");

    // Bind resources to buffer targets
    e_spectral_texture.bind_to(gl::BufferTargetType::eShaderStorage,  0);
    i_validation_buffer.bind_to(gl::BufferTargetType::eShaderStorage, 1);

    // Dispatch shader to validate spectral data
    gl::sync::memory_barrier(gl::BarrierFlags::eShaderStorageBuffer);
    gl::dispatch_compute(m_dispatch);
  }
} // namespace met