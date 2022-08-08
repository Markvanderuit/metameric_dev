#include <metameric/core/knn.hpp>
#include <metameric/core/state.hpp>
#include <metameric/core/texture.hpp>
#include <metameric/components/tasks/task_gen_color_mapping.hpp>
#include <metameric/components/tasks/detail/task_buffer_to_texture2d.hpp>
#include <small_gl/buffer.hpp>
#include <small_gl/texture.hpp>
#include <small_gl/utility.hpp>
#include <small_gl_parser/parser.hpp>
#include <ranges>

namespace met {
  GenColorMappingTask::GenColorMappingTask(const std::string &name, uint mapping_i)
  : detail::AbstractTask(name),
    m_mapping_i(mapping_i) { }

  void GenColorMappingTask::init(detail::TaskInitInfo &info) {
    // Get externally shared resources
    auto &e_app_data = info.get_resource<ApplicationData>(global_key, "app_data");
    auto &e_parser   = info.get_resource<glp::Parser>(global_key, "glsl_parser");

    const uint mapping_n    = e_app_data.loaded_texture.size().prod();
    const uint mapping_ndiv = ceil_div(mapping_n, 256u); 
    const uint mapping_ndiv_sg = ceil_div(mapping_n, 256u / 32u);

    // Initialize objects for color texture gen. through subgroups
    m_mapping_program_sg = {{ .type   = gl::ShaderType::eCompute,
                              .path   = "resources/shaders/task_comp_color_mapping/task_comp_color_mapping.comp",
                              .parser = &e_parser }};
    m_mapping_dispatch_sg = { .groups_x = mapping_ndiv_sg, .bindable_program = &m_mapping_program_sg };

    // Initialize objects for color texture gen.
    m_mapping_program = {{ .type = gl::ShaderType::eCompute,
                           .path = "resources/shaders/mapping_task/apply_color_mapping.comp" }};
    m_mapping_dispatch = { .groups_x = mapping_ndiv, .bindable_program = &m_mapping_program };

    // Set these uniforms once
    m_mapping_program_sg.uniform("u_n",         mapping_n);
    m_mapping_program_sg.uniform("u_mapping_i", m_mapping_i);
    m_mapping_program.uniform("u_n",            mapping_n);
    m_mapping_program.uniform("u_mapping_i",    m_mapping_i);

    // Create buffer target for this task
    gl::Buffer color_buffer = {{ .size = (size_t) mapping_n * sizeof(eig::AlArray3f) }};
    info.insert_resource("color_buffer", std::move(color_buffer));

    // Spawn subtask to create texture from computed buffer object
    gl::Texture2d4f::InfoType ty = { .size = e_app_data.loaded_texture.size() };
    info.emplace_task_after<BufferToTexture2dTask<gl::Texture2d4f>>(
      name(), fmt::format("{}_texture", name()), name(), "color_buffer", ty, "texture");
  }

  void GenColorMappingTask::dstr(detail::TaskDstrInfo &info) {
    // Destroy subtask
    info.remove_task(fmt::format("{}_texture", name()));
  }

  void GenColorMappingTask::eval(detail::TaskEvalInfo &info) {  
    // Get shared resources
    auto &e_spec_buffer    = info.get_resource<gl::Buffer>("gen_spectral_texture", "spectrum_buffer");
    auto &e_mapping_buffer = info.get_resource<gl::Buffer>("gen_spectral_mappings", "mappings_buffer");
    auto &i_color_buffer   = info.get_resource<gl::Buffer>("color_buffer");

    // Bind buffer resources to ssbo targets
    e_spec_buffer.bind_to(gl::BufferTargetType::eShaderStorage,    0);
    e_mapping_buffer.bind_to(gl::BufferTargetType::eShaderStorage, 1);
    i_color_buffer.bind_to(gl::BufferTargetType::eShaderStorage,   2);

    // Dispatch shader to generate color-mapped buffer
    gl::sync::memory_barrier(gl::BarrierFlags::eShaderStorageBuffer);
    gl::dispatch_compute(m_mapping_dispatch_sg);
  }
} // namespace met