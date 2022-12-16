#include <metameric/components/pipeline/task_gen_barycentric_weights.hpp>
#include <metameric/core/detail/trace.hpp>
#include <metameric/core/spectrum.hpp>
#include <metameric/core/data.hpp>
#include <metameric/core/texture.hpp>
#include <small_gl/utility.hpp>
#include <ranges>

namespace met {
  constexpr auto buffer_create_flags = gl::BufferCreateFlags::eMapWrite | gl::BufferCreateFlags::eMapPersistent;
  constexpr auto buffer_access_flags = gl::BufferAccessFlags::eMapWrite | gl::BufferAccessFlags::eMapPersistent | gl::BufferAccessFlags::eMapFlush;

  GenBarycentricWeightsTask::GenBarycentricWeightsTask(const std::string &name)
  : detail::AbstractTask(name) { }

  void GenBarycentricWeightsTask::init(detail::TaskInitInfo &info) {
    met_trace_full();

    // Get shared resources
    auto &e_rgb_texture = info.get_resource<ApplicationData>(global_key, "app_data").loaded_texture;

    const uint generate_n    = e_rgb_texture.size().prod();
    const uint generate_ndiv = ceil_div(generate_n, 256u);

    // Initialize objects for shader call
    m_program = {{ .type = gl::ShaderType::eCompute,
                   .path = "resources/shaders/gen_barycentric_weights/gen_barycentric_weights.comp.spv_opt",
                   .is_spirv_binary = true }};
    m_dispatch = { .groups_x = generate_ndiv, 
                   .bindable_program = &m_program }; 
                   
    // Initialize uniform buffer and writeable, flushable mapping
    m_uniform_buffer = {{ .size = sizeof(UniformBuffer), .flags = buffer_create_flags }};
    m_uniform_map = &m_uniform_buffer.map_as<UniformBuffer>(buffer_access_flags)[0];

    // Initialize buffer holding barycentric weights
    info.emplace_resource<gl::Buffer>("colr_buffer", { .data = cast_span<const std::byte>(io::as_aligned((e_rgb_texture)).data()) });
    info.emplace_resource<gl::Buffer>("bary_buffer", { .size = barycentric_weights * sizeof(float) * generate_n });
  }

  void GenBarycentricWeightsTask::eval(detail::TaskEvalInfo &info) {
    met_trace_full();

    // Continue only on relevant state change
    auto &e_app_data  = info.get_resource<ApplicationData>(global_key, "app_data");
    auto &e_prj_state = e_app_data.project_state;
    guard(e_prj_state.any_verts);

    // Get shared resources
    auto &e_vert_buffer = info.get_resource<gl::Buffer>("gen_spectral_gamut", "vert_buffer");
    auto &e_elem_buffer = info.get_resource<gl::Buffer>("gen_spectral_gamut", "elem_buffer");
    auto &i_colr_buffer = info.get_resource<gl::Buffer>("colr_buffer");
    auto &i_bary_buffer = info.get_resource<gl::Buffer>("bary_buffer");
    
    // Update uniform data
    m_uniform_map->n       = e_app_data.loaded_texture.size().prod();
    m_uniform_map->n_verts = e_app_data.project_data.gamut_verts.size();
    m_uniform_map->n_elems = e_app_data.project_data.gamut_elems.size();
    m_uniform_buffer.flush();

    // Bind resources to buffer targets
    e_vert_buffer.bind_to(gl::BufferTargetType::eShaderStorage, 0);
    e_elem_buffer.bind_to(gl::BufferTargetType::eShaderStorage, 1);
    i_colr_buffer.bind_to(gl::BufferTargetType::eShaderStorage, 2);
    i_bary_buffer.bind_to(gl::BufferTargetType::eShaderStorage, 3);
    m_uniform_buffer.bind_to(gl::BufferTargetType::eUniform,    0);

    // Dispatch shader to generate spectral data
    gl::sync::memory_barrier(gl::BarrierFlags::eShaderStorageBuffer);
    gl::dispatch_compute(m_dispatch);
  }
} // namespace met