#include <metameric/components/views/detail/task_texture_resample.hpp>
#include <small_gl/texture.hpp>
#include <small_gl/utility.hpp>

namespace met::detail {
  template <typename Ty>
  void TextureResampleTask<Ty>::init(SchedulerHandle &info) {
    met_trace_full();

    // Initialize shader object
    m_program = {{ .type = gl::ShaderType::eCompute,
                    .path = "resources/shaders/misc/texture_resample.comp" }};
    m_program.uniform("u_lrgb_to_srgb", static_cast<uint>(m_info.lrgb_to_srgb));

    // Delegate remainder of initialization to set_... functions
    auto _info = m_info;
    m_info.texture_info = { };
    set_sampler_info(info, _info.sampler_info);
    set_texture_info(info, _info.texture_info);
  }

  template <typename Ty>
  bool TextureResampleTask<Ty>::is_active(SchedulerHandle &info) {
    met_trace_full();
    return info(m_info.input_key.first, m_info.input_key.second).is_mutated() || m_is_resized;
  }

  template <typename Ty>
  void TextureResampleTask<Ty>::eval(SchedulerHandle &info) {
    met_trace_full();

    // Get shared resources
    const auto &e_rsrc = info(m_info.input_key.first, m_info.input_key.second).read_only<Ty>();
    auto &i_rsrc       = info(m_info.output_key).writeable<Ty>();

    // Bind resources
    m_sampler.bind_to(0);
    e_rsrc.bind_to(gl::TextureTargetType::eTextureUnit,    0);
    i_rsrc.bind_to(gl::TextureTargetType::eImageWriteOnly, 0);
    gl::sync::memory_barrier(gl::BarrierFlags::eTextureFetch);

    // Dispatch shader, sampling texture into texture image
    gl::dispatch_compute(m_dispatch);
    
    m_is_resized = false;
  }

  template <typename Ty>
  void TextureResampleTask<Ty>::set_texture_info(SchedulerHandle &info, Ty::InfoType texture_info) {
    met_trace_full();

    // Check output texture size has changed
    guard(!m_info.texture_info.size.isApprox(texture_info.size));
    m_info.texture_info = texture_info;

    // Emplace texture resource using new info object; scheduler replaces pre-existing resource
    info(m_info.output_key).init<Ty, Ty::InfoType>(m_info.texture_info);

    // Compute nr. of workgroups as nearest upper divide of n / (16, 16), implying wg size of 256
    eig::Array2u dispatch_n    = m_info.texture_info.size;
    eig::Array2u dispatch_ndiv = ceil_div(dispatch_n, 16u);

    // Initialize objects for texture-to-texture resampling
    m_dispatch = { .groups_x = dispatch_ndiv.x(),
                    .groups_y = dispatch_ndiv.y(),
                    .bindable_program = &m_program };
    m_program.uniform("u_size", dispatch_n);

    m_is_resized = true;
  }

  template <typename Ty>
  void TextureResampleTask<Ty>::set_sampler_info(SchedulerHandle &info, gl::Sampler::InfoType sampler_info) {
    met_trace_full();

    m_info.sampler_info = sampler_info;

    m_sampler = { m_info.sampler_info };
    m_program.uniform("u_sampler", 0);
  }

  /* Explicit template instantiations for TextureResampleTask<> */

  template class TextureResampleTask<gl::Texture2d3f>;
  template class TextureResampleTask<gl::Texture2d4f>;
} // namespace met::detail