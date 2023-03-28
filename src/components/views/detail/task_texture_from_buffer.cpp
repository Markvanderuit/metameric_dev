#include <metameric/components/views/detail/task_texture_from_buffer.hpp>
#include <small_gl/texture.hpp>
#include <small_gl/utility.hpp>

namespace met::detail {
  template <typename Ty>
  void TextureFromBufferTask<Ty>::init(SchedulerHandle &info) {
    met_trace_full();

    // Emplace texture resource using provided info object
    info(m_info.output_key).init<Ty, Ty::InfoType>(m_info.texture_info);
    
    // Compute nr. of workgroups as nearest upper divide of n / (16, 16), implying wg size of 256
    eig::Array2u dispatch_n    = m_info.texture_info.size;
    eig::Array2u dispatch_ndiv = ceil_div(dispatch_n, 16u);

    // Initialize objects for texture-to-texture resampling
    m_program = {{ .type       = gl::ShaderType::eCompute,
                    .spirv_path = "resources/shaders/misc/buffer_to_texture_rgba32f.comp.spv",
                    .cross_path = "resources/shaders/misc/buffer_to_texture_rgba32f.comp.json" }};
    m_dispatch = { .groups_x = dispatch_ndiv.x(),
                   .groups_y = dispatch_ndiv.y(),
                   .bindable_program = &m_program };
    m_program.uniform("u_size", dispatch_n);
  }

  template <typename Ty>
  bool TextureFromBufferTask<Ty>::is_active(SchedulerHandle &info) {
    met_trace_full();
    auto rsrc = info(m_info.input_key.first, m_info.input_key.second);
    return rsrc.is_init() && rsrc.is_mutated();
  }

  template <typename Ty>
  void TextureFromBufferTask<Ty>::eval(SchedulerHandle &info) {
    met_trace_full();

    // Bind buffer/image resources, then dispatch shader to perform copy
    m_program.bind("b_buffer", info(m_info.input_key.first, m_info.input_key.second).read_only<gl::Buffer>());
    m_program.bind("i_image",  info(m_info.output_key).writeable<Ty>());
    gl::dispatch_compute(m_dispatch);
  }

  /* Explicit template instantiations for TextureFromBufferTask<> */

  template class TextureFromBufferTask<gl::Texture2d3f>;
  template class TextureFromBufferTask<gl::Texture2d4f>;
} // namespace met::detail