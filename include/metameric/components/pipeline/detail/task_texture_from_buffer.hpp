#pragma once


#include <metameric/core/scheduler.hpp>
#include <metameric/core/detail/trace.hpp>
#include <small_gl/buffer.hpp>
#include <small_gl/dispatch.hpp>
#include <small_gl/program.hpp>
#include <small_gl/texture.hpp>
#include <small_gl/utility.hpp>

namespace met::detail {
  template <class TextureType>
  struct TextureFromBufferTaskCreateInfo {
    using StringPair  = std::pair<std::string, std::string>;
    using TextureInfo = TextureType::InfoType;

    StringPair input_key;           // Key to input resource
    StringPair output_key;          // Key to output resource (key.first is task name)
    TextureInfo texture_info = {};  // Info about output gl texture object
  };

  template <class TextureType>
  class TextureFromBufferTask : public detail::TaskBase {
    using InfoType = TextureFromBufferTaskCreateInfo<TextureType>;
    using StrPair = std::pair<std::string, std::string>;

    InfoType        m_info;
    gl::ComputeInfo m_dispatch;
    gl::Program     m_program;

  public:
    TextureFromBufferTask(InfoType info)
    : m_info(info) { }
    
    void init(detail::TaskInfo &info) override {
      met_trace_full();

      // Emplace texture resource using provided info object
      info.emplace_resource<TextureType, TextureType::InfoType>(m_info.output_key.second, m_info.texture_info);
      info.emplace_resource<bool>("activate_flag", false);
      
      // Compute nr. of workgroups as nearest upper divide of n / (16, 16), implying wg size of 256
      eig::Array2u dispatch_n    = m_info.texture_info.size;
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

    void eval(detail::TaskInfo &info) override {
      met_trace_full();

      // guard(info.has_resource(m_info.input_key.first, m_info.input_key.second));
      // guard(info.get_resource<bool>("activate_flag"));

      // Get shared resources
      auto &e_rsrc = info.get_resource<gl::Buffer>(m_info.input_key.first, m_info.input_key.second);
      auto &i_rsrc = info.get_resource<TextureType>(m_info.output_key.second);

      // Bind resources to correct buffer/image targets
      e_rsrc.bind_to(gl::BufferTargetType::eShaderStorage,   0);
      i_rsrc.bind_to(gl::TextureTargetType::eImageWriteOnly, 0);
      gl::sync::memory_barrier(gl::BarrierFlags::eShaderStorageBuffer);

      // Dispatch shader, copying buffer into texture object
      gl::dispatch_compute(m_dispatch);
    }
  };
} // namespace met::detail