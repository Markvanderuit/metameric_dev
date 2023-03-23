#include <metameric/core/data.hpp>
#include <metameric/core/mesh.hpp>
#include <metameric/core/state.hpp>
#include <metameric/core/detail/trace.hpp>
#include <metameric/components/pipeline/task_gen_color_mappings.hpp>
#include <small_gl/utility.hpp>

namespace met {
  constexpr auto buffer_create_flags = gl::BufferCreateFlags::eMapWritePersistent;
  constexpr auto buffer_access_flags = gl::BufferAccessFlags::eMapWritePersistent | gl::BufferAccessFlags::eMapFlush;
  constexpr uint buffer_init_size    = 1024u;

  GenColorMappingTask::GenColorMappingTask(uint mapping_i)
  : m_mapping_i(mapping_i) { }

  void GenColorMappingTask::init(SchedulerHandle &info) {
    met_trace_full();

    // Get shared resources
    const auto &e_appl_data = info.resource<ApplicationData>(global_key, "app_data");
    
    // Determine dispatch group size
    const uint mapping_n    = e_appl_data.loaded_texture_f32.size().prod();
    const uint mapping_ndiv = ceil_div(mapping_n, 256u);

    // Initialize objects for convex-combination mapping
    m_program = {{ .type = gl::ShaderType::eCompute,
                   .path = "resources/shaders/gen_color_mappings/gen_color_mapping.comp.spv_opt",
                   .is_spirv_binary = true }};
    m_dispatch = { .groups_x = mapping_ndiv, .bindable_program = &m_program };

    // Set up gamut buffer and establish a flushable mapping
    m_gamut_buffer = {{ .size = buffer_init_size * sizeof(AlColr), .flags = buffer_create_flags }};
    m_gamut_map    = m_gamut_buffer.map_as<AlColr>(buffer_access_flags);

    // Set up uniform buffer and establish a flushable mapping
    m_uniform_buffer = {{ .size = sizeof(UniformBuffer), .flags = buffer_create_flags }};
    m_uniform_map    = m_uniform_buffer.map_as<UniformBuffer>(buffer_access_flags).data();

    // Create color buffer output for this task
    info.emplace_resource<gl::Buffer>("colr_buffer", {
      .size  = (size_t) mapping_n * sizeof(AlColr),
      .flags = gl::BufferCreateFlags::eMapRead 
    });

    m_init_stale = true;
  }

  bool GenColorMappingTask::eval_state(SchedulerHandle &info) {
    const auto &e_pipe_state = info.resource<ProjectState>("state", "pipeline_state");
    return m_init_stale || e_pipe_state.csys[m_mapping_i] || e_pipe_state.any_verts;
  }

  void GenColorMappingTask::eval(SchedulerHandle &info) {
    met_trace_full();

    // Get external resources
    const auto &e_pipe_state  = info.resource<ProjectState>("state", "pipeline_state");
    const auto &e_appl_data   = info.resource<ApplicationData>(global_key, "app_data");
    const auto &e_proj_data   = e_appl_data.project_data;
    const auto &e_bary_buffer = info.resource<gl::Buffer>("gen_delaunay_weights", "bary_buffer");
    const auto &e_tetr_buffer = info.resource<gl::Buffer>("gen_spectral_data", "tetr_buffer");
    const auto &e_vert_spec   = info.resource<std::vector<Spec>>("gen_spectral_data", "vert_spec");
    const auto &e_delaunay    = info.resource<AlignedDelaunayData>("gen_spectral_data", "delaunay");

    // Get modified resources
    auto &i_colr_buffer = info.use_resource<gl::Buffer>("colr_buffer");

    // Update uniform data
    m_uniform_map->n       = e_appl_data.loaded_texture_f32.size().prod();
    m_uniform_map->n_verts = e_delaunay.verts.size();
    m_uniform_map->n_elems = e_delaunay.elems.size();
    m_uniform_buffer.flush();
  
    // Update gamut data, given any state change
    ColrSystem csys = e_proj_data.csys(m_mapping_i);
    for (uint i = 0; i < e_proj_data.vertices.size(); ++i) {
      guard_continue(m_init_stale || e_pipe_state.csys[m_mapping_i] || e_pipe_state.verts[i].any);
      m_gamut_map[i] = csys.apply_color_indirect(e_vert_spec[i]);
      m_gamut_buffer.flush(sizeof(AlColr), i * sizeof(AlColr));
    }

    // Bind required buffers to corresponding targets
    m_uniform_buffer.bind_to(gl::BufferTargetType::eUniform,     0);
    e_bary_buffer.bind_to(gl::BufferTargetType::eShaderStorage,  0);
    m_gamut_buffer.bind_to(gl::BufferTargetType::eShaderStorage, 1);
    e_tetr_buffer.bind_to(gl::BufferTargetType::eShaderStorage,  2);
    i_colr_buffer.bind_to(gl::BufferTargetType::eShaderStorage,  3);

    // Dispatch shader to generate color-mapped buffer
    gl::dispatch_compute(m_dispatch);

    m_init_stale = false;
  }

  void GenColorMappingsTask::init(SchedulerHandle &info) {
    met_trace_full();

    // Get shared resources
    const auto &e_appl_data = info.resource<ApplicationData>(global_key, "app_data");
    uint e_mappings_n   = e_appl_data.project_data.color_systems.size();
    auto e_texture_size = e_appl_data.loaded_texture_f32.size();

    std::string parent_key = info.task_key();

    // Add subtasks to perform mapping
    m_mapping_subtasks.init(info, e_mappings_n, 
      [](uint i)         { return fmt::format("gen_mapping_{}", i); },
      [](auto &, uint i) { return GenColorMappingTask(i); });
  }

  void GenColorMappingsTask::eval(SchedulerHandle &info) {
    met_trace_full();
    
    // Get shared resources
    const auto &e_appl_data = info.resource<ApplicationData>(global_key, "app_data");
    uint e_mappings_n = e_appl_data.project_data.color_systems.size();

    // Adjust nr. of subtasks
    m_mapping_subtasks.eval(info, e_mappings_n);
  }
} // namespace met