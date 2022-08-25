#include <metameric/core/state.hpp>
#include <metameric/core/detail/trace.hpp>
#include <metameric/components/tasks/task_gen_color_mappings.hpp>
#include <small_gl/buffer.hpp>
#include <small_gl/utility.hpp>
#include <small_gl_parser/parser.hpp>

namespace met {
  GenColorMappingTask::GenColorMappingTask(const std::string &name, uint mapping_i)
  : detail::AbstractTask(name, true),
    m_mapping_i(mapping_i) { }

  void GenColorMappingTask::init(detail::TaskInitInfo &info) {
    met_trace_full();

    // Get shared resources
    auto &e_app_data = info.get_resource<ApplicationData>(global_key, "app_data");
    auto &e_parser   = info.get_resource<glp::Parser>(global_key, "glsl_parser");

    // Determine dispatch group size
    const uint mapping_n       = e_app_data.loaded_texture.size().prod();
    // const uint mapping_ndiv    = ceil_div(mapping_n, 256u); 
    const uint mapping_ndiv_sg = ceil_div(mapping_n, 256u / 8u);

    // Initialize objects for mapping through subgroup path
    m_program_sg = {{ .type   = gl::ShaderType::eCompute,
                      .path   = "resources/shaders/gen_color_mappings/gen_color_mapping_cl.comp",
                      .parser = &e_parser }};
    m_dispatch_sg = { .groups_x = mapping_ndiv_sg, .bindable_program = &m_program_sg };

    // Initialize objects for mapping through per-invoc path
    // m_program = {{ .type = gl::ShaderType::eCompute,
    //                .path = "resources/shaders/gen_color_mappings/gen_color_mapping_in.comp" }};
    // m_dispatch = { .groups_x = mapping_ndiv, .bindable_program = &m_program };

    // Set these uniforms once
    m_program_sg.uniform("u_n",         mapping_n);
    m_program_sg.uniform("u_mapping_i", m_mapping_i);
    // m_program.uniform("u_n",            mapping_n);
    // m_program.uniform("u_mapping_i",    m_mapping_i);
    
    // Create color buffer output for this task
    info.emplace_resource<gl::Buffer>("color_buffer", {
      .size  = (size_t) mapping_n * sizeof(eig::AlArray3f),
      .flags = gl::BufferCreateFlags::eMapRead 
    });
  }

  void GenColorMappingTask::eval(detail::TaskEvalInfo &info) {
    met_trace_full();

    // Get shared resources
    auto &e_spec_buffer = info.get_resource<gl::Buffer>("gen_spectral_texture", "spectrum_buffer");
    auto &e_mapp_buffer = info.get_resource<gl::Buffer>("gen_spectral_mappings", "mappings_buffer");
    auto &i_colr_buffer = info.get_resource<gl::Buffer>("color_buffer");

    // Bind buffer resources to ssbo targets
    e_spec_buffer.bind_to(gl::BufferTargetType::eShaderStorage, 0);
    e_mapp_buffer.bind_to(gl::BufferTargetType::eShaderStorage, 1);
    i_colr_buffer.bind_to(gl::BufferTargetType::eShaderStorage, 2);
    gl::sync::memory_barrier(gl::BarrierFlags::eShaderStorageBuffer);

    // Dispatch shader to generate color-mapped buffer
    gl::dispatch_compute(m_dispatch_sg);
  }

  GenColorMappingsTask::GenColorMappingsTask(const std::string &name)
  : detail::AbstractTask(name) { }

  void GenColorMappingsTask::init(detail::TaskInitInfo &info) {
    met_trace_full();

    // Get shared resources
    auto &e_app_data    = info.get_resource<ApplicationData>(global_key, "app_data");
    uint e_mappings_n   = e_app_data.loaded_mappings.size();
    auto e_texture_size = e_app_data.loaded_texture.size();

    // Add subtasks to take mapping and format it into gl::Texture2d4f
    m_texture_subtasks.init(name(), info, e_mappings_n,
      [=](auto &, uint i) { return TextureSubTask({ fmt::format("gen_color_mapping_{}", i), "color_buffer" },
                                                  { fmt::format("gen_color_mapping_texture_{}", i), "texture" },
                                                  { .size = e_texture_size }); },
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
    auto &e_app_data  = info.get_resource<ApplicationData>(global_key, "app_data");
    uint e_mappings_n = e_app_data.loaded_mappings.size();

    // Adjust nr. of subtasks
    m_texture_subtasks.eval(info, e_mappings_n);
    m_mapping_subtasks.eval(info, e_mappings_n);
  }
} // namespace met