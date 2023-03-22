#include <metameric/core/data.hpp>
#include <metameric/core/mesh.hpp>
#include <metameric/core/state.hpp>
#include <metameric/core/spectrum.hpp>
#include <metameric/core/texture.hpp>
#include <metameric/core/detail/trace.hpp>
#include <metameric/components/pipeline/task_gen_delaunay_weights.hpp>
#include <small_gl/utility.hpp>
#include <algorithm>
#include <execution>
#include <ranges>

namespace met {
  constexpr auto buffer_create_flags = gl::BufferCreateFlags::eMapWritePersistent;
  constexpr auto buffer_access_flags = gl::BufferAccessFlags::eMapWritePersistent | gl::BufferAccessFlags::eMapFlush;
  constexpr uint buffer_init_size    = 1024u;

  void GenDelaunayWeightsTask::init(SchedulerHandle &info) {
    met_trace_full();

    // Get shared resources
    const auto &e_rgb_texture = info.resource<ApplicationData>(global_key, "app_data").loaded_texture_f32;
    const auto &e_appl_data   = info.resource<ApplicationData>(global_key, "app_data");
    const auto &e_proj_data   = e_appl_data.project_data;

    const uint generate_n    = e_rgb_texture.size().prod();
    const uint generate_ndiv = ceil_div(generate_n, 256u);

    // Initialize objects for shader call
    m_program = {{ .type = gl::ShaderType::eCompute,
                   .path = "resources/shaders/gen_barycentric_weights/gen_delaunay_weights.comp.spv_opt",
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
    info.emplace_resource<gl::Buffer>("elem_buffer", { .size = buffer_init_size * sizeof(eig::Array4u), .flags = buffer_create_flags });
    info.emplace_resource<gl::Buffer>("bary_buffer", { .size = generate_n * sizeof(eig::Array4f) });
  }

  void GenDelaunayWeightsTask::eval(SchedulerHandle &info) {
    met_trace_full();

    // Continue only on relevant state change
    const auto &e_pipe_state = info.resource<ProjectState>("state", "pipeline_state");
    guard(e_pipe_state.any_verts);

    // Get external resources
    const auto &e_appl_data   = info.resource<ApplicationData>(global_key, "app_data");
    const auto &e_vert_buffer = info.resource<gl::Buffer>("gen_spectral_data", "vert_buffer");
    const auto &e_tetr_buffer = info.resource<gl::Buffer>("gen_spectral_data", "tetr_buffer");
    const auto &e_delaunay    = info.resource<AlignedDelaunayData>("gen_spectral_data", "delaunay");
    
    // Get modified resources
    auto &i_colr_buffer = info.use_resource<gl::Buffer>("colr_buffer");
    auto &i_bary_buffer = info.use_resource<gl::Buffer>("bary_buffer");

    // Update uniform data
    m_uniform_map->n       = e_appl_data.loaded_texture_f32.size().prod();
    m_uniform_map->n_verts = e_delaunay.verts.size();
    m_uniform_map->n_elems = e_delaunay.elems.size();
    m_uniform_buffer.flush();

    // Bind required buffers to corresponding targets
    m_uniform_buffer.bind_to(gl::BufferTargetType::eUniform,    0);
    e_vert_buffer.bind_to(gl::BufferTargetType::eShaderStorage, 0);
    e_tetr_buffer.bind_to(gl::BufferTargetType::eShaderStorage, 1);
    i_colr_buffer.bind_to(gl::BufferTargetType::eShaderStorage, 2);
    i_bary_buffer.bind_to(gl::BufferTargetType::eShaderStorage, 3);

    // Dispatch shader to generate unnormalized barycentric weights in i_bary_buffer
    gl::dispatch_compute(m_dispatch);
  }
} // namespace met