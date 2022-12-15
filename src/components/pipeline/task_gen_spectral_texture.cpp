#include <metameric/components/pipeline/task_gen_spectral_texture.hpp>
#include <metameric/core/detail/trace.hpp>
#include <metameric/core/spectrum.hpp>
#include <metameric/core/state.hpp>
#include <metameric/core/texture.hpp>
#include <small_gl/utility.hpp>
#include <ranges>

namespace met {
  constexpr auto buffer_create_flags = gl::BufferCreateFlags::eMapWrite | gl::BufferCreateFlags::eMapPersistent;
  constexpr auto buffer_access_flags = gl::BufferAccessFlags::eMapWrite | gl::BufferAccessFlags::eMapPersistent | gl::BufferAccessFlags::eMapFlush;

  GenSpectralTextureTask::GenSpectralTextureTask(const std::string &name)
  : detail::AbstractTask(name) { }

  void GenSpectralTextureTask::init(detail::TaskInitInfo &info) {
    met_trace_full();

    // Get shared resources
    auto &e_rgb_texture = info.get_resource<ApplicationData>(global_key, "app_data").loaded_texture;

    const uint generate_n       = e_rgb_texture.size().prod();
    const uint generate_ndiv    = ceil_div(generate_n, 256u);
    const uint generate_ndiv_cl = ceil_div(generate_n, 256u / ceil_div(wavelength_samples, 4u));

    // Initialize objects for clustered shader call
    m_program = {{ .type = gl::ShaderType::eCompute,
                   .path = "resources/shaders/gen_spectral_texture/gen_spectral_texture.comp.spv_opt",
                   .is_spirv_binary = true }};
    m_program_cl = {{ .type = gl::ShaderType::eCompute,
                      .path = "resources/shaders/gen_spectral_texture/gen_spectral_texture.comp.spv_opt",
                      // .path = "resources/shaders/gen_spectral_texture/gen_spectral_texture_mvc_cl.comp.spv_opt",
                      .is_spirv_binary = true }};
    m_dispatch = { .groups_x = generate_ndiv, 
                   .bindable_program = &m_program }; 
    m_dispatch_cl = { .groups_x = generate_ndiv_cl, 
                      .bindable_program = &m_program_cl }; 

    // Initialize uniform buffer and writeable, flushable mapping
    m_uniform_buffer = {{ .size = sizeof(UniformBuffer), .flags = buffer_create_flags }};
    m_uniform_map = &m_uniform_buffer.map_as<UniformBuffer>(buffer_access_flags)[0];

    // Initialize main color and spectral texture buffers
    info.emplace_resource<gl::Buffer>("colr_buffer", { .data = cast_span<const std::byte>(io::as_aligned((e_rgb_texture)).data()) });
    info.emplace_resource<gl::Buffer>("spec_buffer", { .size  = sizeof(Spec) * generate_n });
  }

  void GenSpectralTextureTask::eval(detail::TaskEvalInfo &info) {
    met_trace_full();

    // Continue only on relevant state change
    auto &e_state_gamut = info.get_resource<std::vector<CacheFlag>>("project_state", "gamut_summary");
    guard(std::ranges::any_of(e_state_gamut, [](auto s) { return s == CacheFlag::eStale; }));

    // Get shared resources
    auto &e_app_data    = info.get_resource<ApplicationData>(global_key, "app_data");
    auto &i_colr_bufer  = info.get_resource<gl::Buffer>("colr_buffer");
    auto &i_spec_buffer = info.get_resource<gl::Buffer>("spec_buffer");
    auto &e_spec_buffer = info.get_resource<gl::Buffer>("gen_spectral_gamut", "spec_buffer");
    auto &e_colr_buffer = info.get_resource<gl::Buffer>("gen_spectral_gamut", "colr_buffer");
    auto &e_elem_buffer = info.get_resource<gl::Buffer>("gen_spectral_gamut", "elem_buffer");
    auto &e_bary_buffer = info.get_resource<gl::Buffer>("gen_barycentric_weights", "bary_buffer");
    
    // Update uniform data
    m_uniform_map->n       = e_app_data.loaded_texture.size().prod();
    m_uniform_map->n_verts = e_app_data.project_data.gamut_colr_i.size();
    m_uniform_map->n_elems = e_app_data.project_data.gamut_elems.size();
    m_uniform_buffer.flush();

    /* // Bind resources to buffer targets
    e_spec_buffer.bind_to(gl::BufferTargetType::eShaderStorage, 0);
    e_colr_buffer.bind_to(gl::BufferTargetType::eShaderStorage, 1);
    e_elem_buffer.bind_to(gl::BufferTargetType::eShaderStorage, 2);
    i_colr_bufer.bind_to(gl::BufferTargetType::eShaderStorage,  3);
    i_spec_buffer.bind_to(gl::BufferTargetType::eShaderStorage, 4);
    m_uniform_buffer.bind_to(gl::BufferTargetType::eUniform,    0);
    
    // Dispatch shader to generate spectral data
    gl::sync::memory_barrier(gl::BarrierFlags::eShaderStorageBuffer);
    gl::dispatch_compute(m_dispatch_cl); */

    // Bind resources to buffer targets
    e_spec_buffer.bind_to(gl::BufferTargetType::eShaderStorage, 0);
    e_bary_buffer.bind_to(gl::BufferTargetType::eShaderStorage, 1);
    i_spec_buffer.bind_to(gl::BufferTargetType::eShaderStorage, 2);
    m_uniform_buffer.bind_to(gl::BufferTargetType::eUniform,    0);
    
    // Dispatch shader to generate spectral data
    gl::sync::memory_barrier(gl::BarrierFlags::eShaderStorageBuffer);
    gl::dispatch_compute(m_dispatch_cl);
  }
} // namespace met