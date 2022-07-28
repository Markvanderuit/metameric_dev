#include <metameric/components/tasks/task_buffer_to_texture2d.hpp>
#include <small_gl/buffer.hpp>
#include <small_gl/texture.hpp>
#include <small_gl/utility.hpp>

namespace met {
  template <class TextureTy, class InfoTy>
  BufferToTextureTask<TextureTy, InfoTy>::BufferToTextureTask(const std::string &task_name,
                                                              const std::string &input_task_key,
                                                              const std::string &input_buffer_key,
                                                              InfoTy             output_texture_info,
                                                              const std::string &output_texture_key) 
  : detail::AbstractTask(task_name),
    m_input_task_key(input_task_key),
    m_input_buffer_key(input_buffer_key),
    m_output_texture_key(output_texture_key),
    m_output_texture_info(output_texture_info) { }

  template <class TextureTy, class InfoTy = TextureTy::InfoType>
  void BufferToTextureTask<TextureTy, InfoTy>::init(detail::TaskInitInfo &info) {
    // Emplace texture resource using provided info object
    info.emplace_resource<TextureTy, InfoTy>(m_output_texture_key, m_output_texture_info);

    // Compute nr. of workgroups as nearest upper divide of n
    glm::uvec2 dispatch_n    = m_output_texture_info.size;
    glm::uvec2 dispatch_ndiv = ceil_div(dispatch_n, glm::uvec2(16));

    // Initialize objects for buffer-to-texture conversion
    m_program = {{ .type = gl::ShaderType::eCompute,
                   .path = "resources/shaders/misc/buffer_to_texture_rgba32f.comp" }};
    m_dispatch = { .groups_x = dispatch_ndiv.x,
                   .groups_y = dispatch_ndiv.y,
                   .bindable_program = &m_program };
                   
    // Set these uniforms once
    m_program.uniform<glm::uvec2>("u_size", dispatch_n);
  }
  
  template <class TextureTy, class InfoTy>
  void BufferToTextureTask<TextureTy, InfoTy>::eval(detail::TaskEvalInfo &info) {
    // Get shared resources
    auto &e_buffer  = info.get_resource<gl::Buffer>(m_input_task_key, m_input_buffer_key);
    auto &i_texture = info.get_resource<TextureTy>(m_output_texture_key);

    // Dispatch shader, copying buffer into texture object
    e_buffer.bind_to(gl::BufferTargetType::eShaderStorage, 0);
    i_texture.bind_to(gl::TextureTargetType::eImageWriteOnly, 0);
    gl::sync::memory_barrier(gl::BarrierFlags::eShaderStorageBuffer);
    gl::dispatch_compute(m_dispatch);
  }
} // namespace met