#include <metameric/core/data.hpp>
#include <metameric/core/state.hpp>
#include <metameric/core/detail/trace.hpp>
#include <metameric/components/pipeline/task_gen_color_mappings.hpp>
#include <small_gl/utility.hpp>
#include <small_gl_parser/parser.hpp>

namespace met {
  GenColorMappingTask::GenColorMappingTask(const std::string &name, uint mapping_i)
  : detail::AbstractTask(name, true),
    m_mapping_i(mapping_i) { }

  void GenColorMappingTask::init(detail::TaskInitInfo &info) {
    met_trace_full();

    // Get shared resources
    auto &e_appl_data = info.get_resource<ApplicationData>(global_key, "app_data");
    auto &e_parser   = info.get_resource<glp::Parser>(global_key, "glsl_parser");
    
    // Determine dispatch group size
    const uint mapping_cl      = ceil_div(wavelength_samples, 4u);
    const uint mapping_n       = e_appl_data.loaded_texture.size().prod();
    const uint mapping_ndiv_sg = ceil_div(mapping_n, 256u / mapping_cl);

    // Set up uniform buffer
    std::array<uint, 2> uniform_data = { mapping_n, m_mapping_i };
    m_uniform_buffer = {{ .data = cnt_span<std::byte>(uniform_data) }};

    // Initialize objects for mapping through subgroup path
    m_program_cl = {{ .type   = gl::ShaderType::eCompute,
                      .path   = "resources/shaders/gen_color_mappings/gen_color_mapping_cl.comp.spv_opt",
                      .is_spirv_binary = true }};
    m_dispatch_cl = { .groups_x = mapping_ndiv_sg, .bindable_program = &m_program_cl };

    // Create color buffer output for this task
    info.emplace_resource<gl::Buffer>("colr_buffer", {
      .size  = (size_t) mapping_n * sizeof(eig::AlArray3f),
      .flags = gl::BufferCreateFlags::eMapRead 
    });

    m_init_stale = true;
  }

  void GenColorMappingTask::eval(detail::TaskEvalInfo &info) {
    met_trace_full();

    // Generate color texture only on relevant state changes
    auto &e_pipe_state = info.get_resource<ProjectState>("state", "pipeline_state");
    auto &e_appl_data  = info.get_resource<ApplicationData>(global_key, "app_data");
    bool activate_flag = m_init_stale || e_pipe_state.mapps[m_mapping_i] || e_pipe_state.any_verts;
    info.get_resource<bool>(fmt::format("gen_color_mapping_texture_{}", m_mapping_i), "activate_flag") = activate_flag;
    guard(activate_flag);
    m_init_stale = false;

    // Get shared resources
    auto &e_spec_buffer = info.get_resource<gl::Buffer>("gen_spectral_texture", "spec_buffer");
    auto &e_mapp_buffer = info.get_resource<gl::Buffer>("gen_spectral_mappings", "mapp_buffer");
    auto &i_colr_buffer = info.get_resource<gl::Buffer>("colr_buffer");

    // Bind buffer resources to ssbo targets
    m_uniform_buffer.bind_to(gl::BufferTargetType::eUniform,    0);
    e_spec_buffer.bind_to(gl::BufferTargetType::eShaderStorage, 0);
    e_mapp_buffer.bind_to(gl::BufferTargetType::eShaderStorage, 1);
    i_colr_buffer.bind_to(gl::BufferTargetType::eShaderStorage, 2);
    gl::sync::memory_barrier(gl::BarrierFlags::eShaderStorageBuffer);

    // Dispatch shader to generate color-mapped buffer
    gl::dispatch_compute(m_dispatch_cl);
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