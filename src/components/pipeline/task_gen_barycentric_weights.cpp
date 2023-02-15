#include <metameric/core/data.hpp>
#include <metameric/core/state.hpp>
#include <metameric/core/spectrum.hpp>
#include <metameric/core/texture.hpp>
#include <metameric/core/detail/trace.hpp>
#include <metameric/components/pipeline/task_gen_barycentric_weights.hpp>
#include <small_gl/utility.hpp>
#include <algorithm>
#include <execution>
#include <ranges>

namespace met {
  constexpr auto buffer_create_flags = gl::BufferCreateFlags::eMapWrite | gl::BufferCreateFlags::eMapPersistent;
  constexpr auto buffer_access_flags = gl::BufferAccessFlags::eMapWrite | gl::BufferAccessFlags::eMapPersistent | gl::BufferAccessFlags::eMapFlush;

  GenBarycentricWeightsTask::GenBarycentricWeightsTask(const std::string &name)
  : detail::AbstractTask(name) { }

  void GenBarycentricWeightsTask::init(detail::TaskInitInfo &info) {
    met_trace_full();

    // Get shared resources
    auto &e_rgb_texture = info.get_resource<ApplicationData>(global_key, "app_data").loaded_texture_f32;

    const uint generate_n    = e_rgb_texture.size().prod();
    const uint generate_ndiv = ceil_div(generate_n, 256u / 2u);

    // Initialize objects for shader call
    m_program = {{ .type = gl::ShaderType::eCompute,
                   .path = "resources/shaders/gen_barycentric_weights/gen_barycentric_weights_pre.comp.spv_opt",
                   .is_spirv_binary = true }};
    m_dispatch = { .groups_x = generate_ndiv, 
                   .bindable_program = &m_program }; 

    // Initialize uniform buffer and writeable, flushable mapping
    m_uniform_buffer = {{ .size = sizeof(UniformBuffer), .flags = buffer_create_flags }};
    m_uniform_map = &m_uniform_buffer.map_as<UniformBuffer>(buffer_access_flags)[0];

    // Generate packed texture data
    std::vector<uint> packed_data(e_rgb_texture.size().prod());
    std::transform(std::execution::par_unseq, range_iter(e_rgb_texture.data()), packed_data.begin(), [](const auto &v_) {
      eig::Array3u v = (v_.max(0.f).min(1.f) * 255.f).round().cast<uint>().eval();
      return v[0] | (v[1] << 8) | (v[2] << 16);
    });

    // Initialize buffer holding barycentric weights
    info.emplace_resource<gl::Buffer>("pack_buffer", { .data = cnt_span<const std::byte>(packed_data) });
    info.emplace_resource<gl::Buffer>("colr_buffer", { .data = cast_span<const std::byte>(io::as_aligned((e_rgb_texture)).data()) });
    info.emplace_resource<gl::Buffer>("bary_buffer", { .size = generate_n * sizeof(Bary) });
  }

  void GenBarycentricWeightsTask::dstr(detail::TaskDstrInfo &info) {
    met_trace_full();

    // Unmap buffers
    if (m_uniform_buffer.is_init() && m_uniform_buffer.is_mapped()) 
      m_uniform_buffer.unmap();
  }

  void GenBarycentricWeightsTask::eval(detail::TaskEvalInfo &info) {
    met_trace_full();

    // Continue only on relevant state change
    auto &e_pipe_state = info.get_resource<ProjectState>("state", "pipeline_state");
    guard(e_pipe_state.any_verts);

    // Get shared resources
    auto &e_appl_data   = info.get_resource<ApplicationData>(global_key, "app_data");
    auto &e_vert_buffer = info.get_resource<gl::Buffer>("gen_spectral_gamut", "vert_buffer");
    auto &e_elem_buffer = info.get_resource<gl::Buffer>("gen_spectral_gamut", "elem_buffer");
    auto &i_colr_buffer = info.get_resource<gl::Buffer>("colr_buffer");
    auto &i_bary_buffer = info.get_resource<gl::Buffer>("bary_buffer");
    
    // Update uniform data and bind uniform buffer to correct target
    m_uniform_map->n       = e_appl_data.loaded_texture_f32.size().prod();
    m_uniform_map->n_verts = e_appl_data.project_data.gamut_verts.size();
    m_uniform_map->n_elems = e_appl_data.project_data.gamut_elems.size();
    m_uniform_buffer.flush();
    m_uniform_buffer.bind_to(gl::BufferTargetType::eUniform, 0);

    // Dispatch shader to generate unnormalized barycentric weights in i_bary_buffer
    e_vert_buffer.bind_to(gl::BufferTargetType::eShaderStorage, 0);
    e_elem_buffer.bind_to(gl::BufferTargetType::eShaderStorage, 1);
    i_colr_buffer.bind_to(gl::BufferTargetType::eShaderStorage, 2);
    i_bary_buffer.bind_to(gl::BufferTargetType::eShaderStorage, 3);
    gl::dispatch_compute(m_dispatch);
  }
} // namespace met