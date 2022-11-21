#include <metameric/components/pipeline/task_gen_spectral_texture.hpp>
#include <metameric/core/detail/trace.hpp>
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
    met_trace_full();

    // Get shared resources
    auto &e_rgb_texture = info.get_resource<ApplicationData>(global_key, "app_data").loaded_texture;

    const uint generate_n       = e_rgb_texture.size().prod();
    const uint generate_ndiv_cl = ceil_div(generate_n, 256u / ceil_div(wavelength_samples, 4u));

    // Initialize objects for clustered shader call
    m_program_cl = {{ .type = gl::ShaderType::eCompute,
                      .path = "resources/shaders/gen_spectral_texture/gen_spectral_texture_mvc_cl.comp.spv_opt",
                      .is_spirv_binary = true }};
    m_dispatch_cl = { .groups_x = generate_ndiv_cl, 
                      .bindable_program = &m_program_cl }; 

    // Initialize uniform buffers
    const auto create_flags = gl::BufferCreateFlags::eMapWrite | gl::BufferCreateFlags::eMapPersistent;
    const auto map_flags = gl::BufferAccessFlags::eMapWrite | gl::BufferAccessFlags::eMapPersistent | gl::BufferAccessFlags::eMapFlush;
    m_uniform_buffer = {{ .data = obj_span<const std::byte>(generate_n) }};
    m_bary_buffer = {{ .size = sizeof(BarycentricBuffer), .flags = create_flags}};
    m_bary_map = &m_bary_buffer.map_as<BarycentricBuffer>(map_flags)[0];

    // Initialize main color and spectral texture buffers
    info.emplace_resource<gl::Buffer>("color_buffer", {  .data = cast_span<const std::byte>(io::as_aligned((e_rgb_texture)).data()) });
    info.emplace_resource<gl::Buffer>("spectrum_buffer", { .size  = sizeof(Spec) * generate_n });
  }

  void GenSpectralTextureTask::eval(detail::TaskEvalInfo &info) {
    met_trace_full();

    // Generate spectral texture only on relevant state change
    auto &e_state_spec = info.get_resource<std::array<CacheState, 4>>("project_state", "gamut_spec");
    guard(std::ranges::any_of(e_state_spec, [](auto s) { return s == CacheState::eStale; }));

    // Get shared resources
    auto &e_app_data      = info.get_resource<ApplicationData>(global_key, "app_data");
    auto &e_color_gamut_c = e_app_data.project_data.gamut_colr_i;
    auto &e_spect_gamut_s = info.get_resource<gl::Buffer>("gen_spectral_gamut", "buffer_spec");
    auto &i_color_texture = info.get_resource<gl::Buffer>("color_buffer");
    auto &i_spect_texture = info.get_resource<gl::Buffer>("spectrum_buffer");
    auto &e_color_gamut   = info.get_resource<gl::Buffer>("gen_spectral_gamut", "buffer_colr");
    auto &e_elems_gamut   = info.get_resource<gl::Buffer>("gen_spectral_gamut", "buffer_elem");

    // Update barycentric coordinate inverse matrix in mapped uniform buffer
    m_bary_map->sub = e_color_gamut_c[3];
    m_bary_map->inv.block<3, 3>(0, 0) = (eig::Matrix3f() 
      << e_color_gamut_c[0] - e_color_gamut_c[3], 
         e_color_gamut_c[1] - e_color_gamut_c[3], 
         e_color_gamut_c[2] - e_color_gamut_c[3]).finished().inverse().eval();
    m_bary_buffer.flush();

    // Bind resources to buffer targets
    e_spect_gamut_s.bind_to(gl::BufferTargetType::eShaderStorage, 0);
    e_color_gamut.bind_to(gl::BufferTargetType::eShaderStorage,   1);
    e_elems_gamut.bind_to(gl::BufferTargetType::eShaderStorage,   2);
    i_color_texture.bind_to(gl::BufferTargetType::eShaderStorage, 3);
    i_spect_texture.bind_to(gl::BufferTargetType::eShaderStorage, 4);
    m_uniform_buffer.bind_to(gl::BufferTargetType::eUniform, 0);
    m_bary_buffer.bind_to(gl::BufferTargetType::eUniform,    1);
    
    // Dispatch shader to generate spectral data
    gl::sync::memory_barrier(gl::BarrierFlags::eShaderStorageBuffer);
    gl::dispatch_compute(m_dispatch_cl);
  }
} // namespace met