#include <metameric/core/data.hpp>
#include <metameric/core/state.hpp>
#include <metameric/core/detail/trace.hpp>
#include <metameric/components/pipeline/task_gen_color_mappings.hpp>
#include <small_gl/utility.hpp>
#include <small_gl_parser/parser.hpp>

namespace met {
  constexpr auto buffer_create_flags = gl::BufferCreateFlags::eMapWrite 
                                     | gl::BufferCreateFlags::eMapPersistent;
  constexpr auto buffer_access_flags = gl::BufferAccessFlags::eMapWrite 
                                     | gl::BufferAccessFlags::eMapPersistent
                                     | gl::BufferAccessFlags::eMapFlush;

  GenColorMappingTask::GenColorMappingTask(const std::string &name, uint mapping_i)
  : detail::AbstractTask(name, true),
    m_mapping_i(mapping_i) { }

  void GenColorMappingTask::init(detail::TaskInitInfo &info) {
    met_trace_full();

    // Get shared resources
    auto &e_appl_data = info.get_resource<ApplicationData>(global_key, "app_data");
    
    // Determine dispatch group size
    const uint mapping_n       = e_appl_data.loaded_texture.size().prod();
    const uint mapping_ndiv    = ceil_div(mapping_n, 256u);

    // Initialize objects for convex-combination mapping
    m_program = {{ .type = gl::ShaderType::eCompute,
                   .path = "resources/shaders/gen_color_mappings/gen_color_mapping.comp.spv_opt",
                   .is_spirv_binary = true }};
    m_dispatch = { .groups_x = mapping_ndiv, .bindable_program = &m_program };

    // Set up gamut buffer and establish a flushable mapping
    m_gamut_buffer = {{ .size = barycentric_weights * sizeof(AlColr), .flags = buffer_create_flags }};
    m_gamut_map    = cast_span<AlColr>(m_gamut_buffer.map(buffer_access_flags));

    // Set up uniform buffer and establish a flushable mapping
    m_uniform_buffer = {{ .size = sizeof(UniformBuffer), .flags = buffer_create_flags }};
    m_uniform_map = &m_uniform_buffer.map_as<UniformBuffer>(buffer_access_flags)[0];

    // Create color buffer output for this task
    info.emplace_resource<gl::Buffer>("colr_buffer", {
      .size  = (size_t) mapping_n * sizeof(AlColr),
      .flags = gl::BufferCreateFlags::eMapRead 
    });

    m_init_stale = true;
  }

  void GenColorMappingTask::dstr(detail::TaskDstrInfo &info) {
    if (m_gamut_buffer.is_init() && m_gamut_buffer.is_mapped()) m_gamut_buffer.unmap();
  }

  void GenColorMappingTask::eval(detail::TaskEvalInfo &info) {
    met_trace_full();

    // Generate color texture only on relevant state changes
    auto &e_pipe_state = info.get_resource<ProjectState>("state", "pipeline_state");

    // Continue only on relevant state changes; first time this is always true
    bool activate_flag = m_init_stale || e_pipe_state.mapps[m_mapping_i] || e_pipe_state.any_verts;
    info.get_resource<bool>(fmt::format("gen_color_mapping_texture_{}", m_mapping_i), "activate_flag") = activate_flag;
    guard(activate_flag);
    m_init_stale = false;

    // Get shared resources
    auto &e_appl_data  = info.get_resource<ApplicationData>(global_key, "app_data");
    auto &e_proj_data  = e_appl_data.project_data;
    auto &e_bary_buffer = info.get_resource<gl::Buffer>("gen_barycentric_weights", "bary_buffer");
    auto &e_spec_buffer = info.get_resource<gl::Buffer>("gen_spectral_texture", "spec_buffer");
    auto &e_mapp_buffer = info.get_resource<gl::Buffer>("gen_color_systems", "mapp_buffer");
    auto &i_colr_buffer = info.get_resource<gl::Buffer>("colr_buffer");
    auto &e_gamut_spec  = info.get_resource<std::vector<Spec>>("gen_spectral_gamut", "gamut_spec");

    // Update uniform data
    if (e_pipe_state.any_verts) {
      m_uniform_map->n       = e_appl_data.loaded_texture.size().prod();
      m_uniform_map->n_verts = e_proj_data.gamut_verts.size();
      m_uniform_buffer.flush();
    }

    // Update gamut data, given a mapping/vertex state change
    ColrSystem csys = e_proj_data.csys(m_mapping_i);
    for (uint i = 0; i < e_proj_data.gamut_verts.size(); ++i) {
      guard_continue(e_pipe_state.verts[i].any);
      m_gamut_map[i] = csys(e_gamut_spec[i]);
    }
    m_gamut_buffer.flush();

    // Bind buffer resources to correct buffer targets
    m_uniform_buffer.bind_to(gl::BufferTargetType::eUniform,     0);
    e_bary_buffer.bind_to(gl::BufferTargetType::eShaderStorage,  0);
    m_gamut_buffer.bind_to(gl::BufferTargetType::eShaderStorage, 1);
    i_colr_buffer.bind_to(gl::BufferTargetType::eShaderStorage,  2);

    // Dispatch shader to generate color-mapped buffer
    gl::dispatch_compute(m_dispatch);
  }

  GenColorMappingsTask::GenColorMappingsTask(const std::string &name)
  : detail::AbstractTask(name) { }

  void GenColorMappingsTask::init(detail::TaskInitInfo &info) {
    met_trace_full();

    // Get shared resources
    auto &e_appl_data   = info.get_resource<ApplicationData>(global_key, "app_data");
    uint e_mappings_n   = e_appl_data.project_data.color_systems.size();
    auto e_texture_size = e_appl_data.loaded_texture.size();

    // Add subtasks to take mapping and format it into gl::Texture2d4f
    m_texture_subtasks.init(name(), info, e_mappings_n,
      [=](auto &, uint i) -> TextureSubTask { 
        return {{ .input_key    = { fmt::format("gen_color_mapping_{}", i), "colr_buffer" },
                  .output_key   = { fmt::format("gen_color_mapping_texture_{}", i), "texture" },
                  .texture_info = { .size = e_texture_size }}}; 
      },
      [](auto &, uint i) { return fmt::format("gen_color_mapping_texture_{}", i); });

    // Add subtasks to perform mapping
    m_mapping_subtasks.init(name(), info, e_mappings_n,
      [](auto &, uint i) { return MappingSubTask(fmt::format("gen_color_mapping_{}", i), i); }, 
      [](auto &, uint i) { return fmt::format("gen_color_mapping_{}", i); });
  }

  void GenColorMappingsTask::dstr(detail::TaskDstrInfo &info) {
    met_trace_full();

    // Remove subtasks
    m_texture_subtasks.dstr(info);
    m_mapping_subtasks.dstr(info);
  }

  void GenColorMappingsTask::eval(detail::TaskEvalInfo &info) {
    met_trace_full();
    
    // Get shared resources
    auto &e_appl_data = info.get_resource<ApplicationData>(global_key, "app_data");
    uint e_mappings_n = e_appl_data.project_data.color_systems.size();

    // Adjust nr. of subtasks
    m_texture_subtasks.eval(info, e_mappings_n);
    m_mapping_subtasks.eval(info, e_mappings_n);
  }
} // namespace met