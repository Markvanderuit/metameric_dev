#pragma once


#include <metameric/core/scheduler.hpp>
#include <small_gl/buffer.hpp>
#include <small_gl/dispatch.hpp>
#include <small_gl/program.hpp>
#include <small_gl/texture.hpp>
#include <small_gl/utility.hpp>

namespace met::detail {
  template <class TextureTy, class InfoTy = TextureTy::InfoType>
  class TextureFromBufferTask : public detail::AbstractTask {
    using StrPair = std::pair<std::string, std::string>;

    StrPair         m_input_key;
    StrPair         m_output_key;
    InfoTy          m_texture_info;
    gl::ComputeInfo m_dispatch;
    gl::Program     m_program;

  public:
    TextureFromBufferTask(const StrPair &input_key,
                          const StrPair &output_key,
                          InfoTy         texture_info)
    : detail::AbstractTask(output_key.first, true),
      m_input_key(input_key),
      m_output_key(output_key),
      m_texture_info(texture_info) { }
    
    void init(detail::TaskInitInfo &info) override {
      met_trace();

      // Emplace texture resource using provided info object
      info.emplace_resource<TextureTy, InfoTy>(m_output_key.second, m_texture_info);
      
      // Compute nr. of workgroups as nearest upper divide of n / (16, 16), implying wg size of 256
      eig::Array2u dispatch_n    = m_texture_info.size;
      eig::Array2u dispatch_ndiv = ceil_div(dispatch_n, 16u);

      // Initialize objects for texture-to-texture resampling
      m_program = {{ .type = gl::ShaderType::eCompute,
                     .path = "resources/shaders/misc/buffer_to_texture_rgba32f.comp" }};
      m_dispatch = { .groups_x = dispatch_ndiv.x(),
                     .groups_y = dispatch_ndiv.y(),
                     .bindable_program = &m_program };
                   
      // Set these uniforms once
      m_program.uniform("u_size", dispatch_n);
    }

    void eval(detail::TaskEvalInfo &info) override {
      met_trace();

      // Get shared resources
      guard(info.has_resource(m_input_key.first, m_input_key.second));

      // guard(info.has_resource(m_input_key.first, m_input_key.second));
      auto &e_rsrc = info.get_resource<gl::Buffer>(m_input_key.first, m_input_key.second);
      auto &i_rsrc = info.get_resource<TextureTy>(m_output_key.second);

      // Bind resources to correct buffer/image targets
      e_rsrc.bind_to(gl::BufferTargetType::eShaderStorage,   0);
      i_rsrc.bind_to(gl::TextureTargetType::eImageWriteOnly, 0);
      gl::sync::memory_barrier(gl::BarrierFlags::eShaderStorageBuffer);

      // Dispatch shader, copying buffer into texture object
      gl::dispatch_compute(m_dispatch);
    }
  };
} // namespace met::detail