#include <metameric/core/detail/trace.hpp>
#include <metameric/components/tasks/detail/task_buffer_to_texture2d.hpp>
#include <small_gl/buffer.hpp>
#include <small_gl/texture.hpp>
#include <small_gl/utility.hpp>

namespace met {
  template <class TextureTy, class InfoTy>
  BufferToTexture2dTask<TextureTy, InfoTy>::BufferToTexture2dTask(const std::string &task_name,
                                                                  const std::string &input_task_key,
                                                                  const std::string &input_buffer_key,
                                                                  InfoTy             output_texture_info,
                                                                  const std::string &output_texture_key) 
  : detail::AbstractTask(task_name),
    m_inp_task_key(input_task_key),
    m_inp_rsrc_key(input_buffer_key),
    m_out_rsrc_key(output_texture_key),
    m_out_rsrc_info(output_texture_info) { }

  template <class TextureTy, class InfoTy>
  void BufferToTexture2dTask<TextureTy, InfoTy>::init(detail::TaskInitInfo &info) {
    met_trace_full();

    // Emplace texture resource using provided info object
    info.emplace_resource<TextureTy, InfoTy>(m_out_rsrc_key, m_out_rsrc_info);

    // Compute nr. of workgroups as nearest upper divide of n / (16, 16), implying wg size of 256
    eig::Array2u dispatch_n    = m_out_rsrc_info.size;
    eig::Array2u dispatch_ndiv = ceil_div(dispatch_n, 16u);

    // Initialize objects for buffer-to-texture conversion
    m_program = {{ .type = gl::ShaderType::eCompute,
                   .path = "resources/shaders/misc/buffer_to_texture_rgba32f.comp" }};
    m_dispatch = { .groups_x = dispatch_ndiv.x(),
                   .groups_y = dispatch_ndiv.y(),
                   .bindable_program = &m_program };
                   
    // Set these uniforms once
    m_program.uniform("u_size", dispatch_n);
  }
  
  template <class TextureTy, class InfoTy>
  void BufferToTexture2dTask<TextureTy, InfoTy>::eval(detail::TaskEvalInfo &info) {
    met_trace_full();

    // Get shared resources
    guard(info.has_resource(m_inp_task_key, m_inp_rsrc_key));
    auto &e_rsrc = info.get_resource<gl::Buffer>(m_inp_task_key, m_inp_rsrc_key);
    auto &i_rsrc = info.get_resource<TextureTy>(m_out_rsrc_key);

    // Bind resources to correct buffer/image targets
    e_rsrc.bind_to(gl::BufferTargetType::eShaderStorage,   0);
    i_rsrc.bind_to(gl::TextureTargetType::eImageWriteOnly, 0);
    gl::sync::memory_barrier(gl::BarrierFlags::eShaderStorageBuffer);

    // Dispatch shader, copying buffer into texture object
    gl::dispatch_compute(m_dispatch);
  }

  /* Explicit template instantiations for common types */
  template class BufferToTexture2dTask<gl::Texture2d1f>;
  template class BufferToTexture2dTask<gl::Texture2d2f>;
  template class BufferToTexture2dTask<gl::Texture2d4f>;
  template class BufferToTexture2dTask<gl::Texture2d4f>;
} // namespace met