#include <metameric/editor/detail/task_texture_resample.hpp>
#include <small_gl/buffer.hpp>
#include <small_gl/texture.hpp>
#include <small_gl/utility.hpp>

namespace met::detail {
  template <typename Ty>
  void TextureResampleTask<Ty>::init(SchedulerHandle &info) {
    met_trace_full();

    // Initialize program object in cache
    std::tie(m_program_key, std::ignore) = info.global("cache").getw<gl::detail::ProgramCache>().set({{ 
      .type       = gl::ShaderType::eCompute,
      .spirv_path = "shaders/misc/texture_resample.comp.spv",
      .cross_path = "shaders/misc/texture_resample.comp.json"
    }});

    // Initialize uniform buffer and writeable, flushable mapping
    std::tie(m_uniform_buffer, m_uniform_map) = gl::Buffer::make_flusheable_object<UniformBuffer>();
    m_uniform_map->lrgb_to_srgb = m_info.lrgb_to_srgb;

    // Delegate remainder of initialization to set_... functions
    auto _info = m_info;
    m_info.texture_info = { };
    set_sampler_info(info, _info.sampler_info);
    set_texture_info(info, _info.texture_info);
  }

  template <typename Ty>
  bool TextureResampleTask<Ty>::is_active(SchedulerHandle &info) {
    met_trace();
    return m_is_mutated || info(m_info.input_key.first, m_info.input_key.second).is_mutated();
  }

  template <typename Ty>
  void TextureResampleTask<Ty>::eval(SchedulerHandle &info) {
    met_trace_full();

    // Draw relevant program from cache
    auto &program = info.global("cache").getw<gl::detail::ProgramCache>().at(m_program_key);

    // Bind image/sampler resources and program
    program.bind();
    program.bind("b_uniform", m_uniform_buffer);
    program.bind("s_image_r", m_sampler);
    program.bind("s_image_r", info(m_info.input_key.first, m_info.input_key.second).template getr<Ty>());
    program.bind("i_image_w", info(m_info.output_key).template getw<Ty>());
    
    // Then dispatch shader to perform resample
    gl::dispatch_compute(m_dispatch);
    
    m_is_mutated = false;
  }

  template <typename Ty>
  void TextureResampleTask<Ty>::set_texture_info(SchedulerHandle &info, Ty::InfoType texture_info) {
    met_trace_full();

    // Check output texture size has changed
    guard(!m_info.texture_info.size.isApprox(texture_info.size));
    m_info.texture_info = texture_info;

    // Emplace texture resource using new info object; scheduler replaces pre-existing resource
    info(m_info.output_key).template init<Ty, typename Ty::InfoType>(m_info.texture_info);

    // Compute nr. of workgroups as nearest upper divide of n / (16, 16), implying wg size of 256
    eig::Array2u dispatch_n    = m_info.texture_info.size;
    eig::Array2u dispatch_ndiv = ceil_div(dispatch_n, 16u);

    m_dispatch = { .groups_x = dispatch_ndiv.x(), .groups_y = dispatch_ndiv.y() };

    m_uniform_map->size = dispatch_n;
    m_uniform_buffer.flush();

    m_is_mutated = true;
  }

  template <typename Ty>
  void TextureResampleTask<Ty>::set_sampler_info(SchedulerHandle &info, gl::Sampler::InfoType sampler_info) {
    met_trace_full();

    m_info.sampler_info = sampler_info;
    m_sampler = { m_info.sampler_info };
    m_is_mutated = true;
  }

  /* Explicit template instantiations for TextureResampleTask<> */

  template class TextureResampleTask<gl::Texture2d3f>;
  template class TextureResampleTask<gl::Texture2d4f>;
} // namespace met::detail