#include <metameric/core/knn.hpp>
#include <metameric/core/state.hpp>
#include <metameric/core/texture.hpp>
#include <metameric/components/tasks/mapping_task.hpp>
#include <small_gl/utility.hpp>
#include <ranges>

namespace met {
  MappingTask::MappingTask(const std::string &name)
  : detail::AbstractTask(name) { }

  void MappingTask::init(detail::TaskInitInfo &info) {
    // Get externally shared resources
    auto &e_app_data = info.get_resource<ApplicationData>(global_key, "app_data");

    // Temporary object to describe circumstances for spectral to rgb conversion
    struct MappingType {
      CMFS cmfs          = models::cmfs_srgb;
      Spec illuminant    = models::emitter_cie_fl11;
      uint n_scatterings = 0;
    } mapping;

    const uint mapping_n    = e_app_data.rgb_texture.size().prod();
    const uint mapping_ndiv = ceil_div(mapping_n, 256u); 

    // Initialize objects for color texture gen.
    m_mapping_buffer  = {{ .data = std::span { (const std::byte *) &mapping, sizeof(mapping) }}};
    m_mapping_program = {{ .type = gl::ShaderType::eCompute,
                           .path = "resources/shaders/mapping_task/apply_color_mapping.comp" }};
    m_mapping_dispatch = { .groups_x = mapping_ndiv, .bindable_program = &m_mapping_program };

    glm::uvec2 texture_n    = { e_app_data.rgb_texture.size().x(), e_app_data.rgb_texture.size().y() };
    glm::uvec2 texture_ndiv = ceil_div(texture_n, glm::uvec2(16));

    // Initialize objects for buffer-to-texture conversion
    m_texture_program = {{ .type = gl::ShaderType::eCompute,
                           .path = "resources/shaders/mapping_task/buffer_to_texture.comp" }};
    m_texture_dispatch = { .groups_x = texture_ndiv.x,
                           .groups_y = texture_ndiv.y,
                           .bindable_program = &m_texture_program };

    // Set these uniforms once
    m_mapping_program.uniform<uint>("u_n", mapping_n);
    m_texture_program.uniform<glm::uvec2>("u_size", texture_n);

    // Create buffer target for this task
    gl::Buffer color_buffer = {{ .size = (size_t) mapping_n * sizeof(eig::AlArray3f) }};
    info.insert_resource("color_buffer", std::move(color_buffer));

    // Create texture target for this task
    gl::Texture2d4f color_texture = {{ .size = texture_n }};
    info.insert_resource("color_texture", std::move(color_texture));
  }

  void MappingTask::eval(detail::TaskEvalInfo &info) {
    // Get shared resources
    auto &e_spectral_texture_buffer = info.get_resource<gl::Buffer>("generate_spectral", 
                                                                    "spectral_texture_buffer");
    auto &i_color_texture           = info.get_resource<gl::Texture2d4f>("color_texture");
    auto &i_color_buffer            = info.get_resource<gl::Buffer>("color_buffer");

    // Generate color texture buffer
    e_spectral_texture_buffer.bind_to(gl::BufferTargetType::eShaderStorage, 0);
    m_mapping_buffer.bind_to(gl::BufferTargetType::eShaderStorage,          1);
    i_color_buffer.bind_to(gl::BufferTargetType::eShaderStorage,            2);
    gl::sync::memory_barrier(gl::BarrierFlags::eShaderStorageBuffer);
    gl::dispatch_compute(m_mapping_dispatch);

    // Render to texture
    i_color_buffer.bind_to(gl::BufferTargetType::eShaderStorage,    0);
    i_color_texture.bind_to(gl::TextureTargetType::eImageWriteOnly, 0);
    gl::sync::memory_barrier(gl::BarrierFlags::eShaderStorageBuffer);
    gl::dispatch_compute(m_texture_dispatch);
  }
} // namespace met