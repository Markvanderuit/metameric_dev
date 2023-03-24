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

    StringPair  input_key;             // Key to input resource
    std::string output_key;            // Key to output resource
    TextureInfo texture_info = {};     // Info about output gl texture object
  };

  template <class TextureType>
  class TextureFromBufferTask : public detail::TaskNode {
    using InfoType = TextureFromBufferTaskCreateInfo<TextureType>;
    using StrPair = std::pair<std::string, std::string>;

    InfoType        m_info;
    gl::ComputeInfo m_dispatch;
    gl::Program     m_program;

  public:
    TextureFromBufferTask(InfoType info)
    : m_info(info) { }
    
    void init(SchedulerHandle &info) override {
      met_trace_full();

      // Emplace texture resource using provided info object
      info.resource(m_info.output_key).init<TextureType, TextureType::InfoType>(m_info.texture_info);
      
      // Compute nr. of workgroups as nearest upper divide of n / (16, 16), implying wg size of 256
      eig::Array2u dispatch_n    = m_info.texture_info.size;
      eig::Array2u dispatch_ndiv = ceil_div(dispatch_n, 16u);

      // Initialize objects for texture-to-texture resampling
      m_program = {{ .type = gl::ShaderType::eCompute,
                     .path = "resources/shaders/misc/buffer_to_texture_rgba32f.comp" }};
      m_dispatch = { .groups_x = dispatch_ndiv.x(),
                     .groups_y = dispatch_ndiv.y(),
                     .bindable_program = &m_program };
      m_program.uniform("u_size", dispatch_n);
    }

    bool eval_state(SchedulerHandle &info) override {
      met_trace_full();

      // Run computation only if input exists and has been modified
      auto rsrc = info.resource(m_info.input_key.first, m_info.input_key.second);
      return rsrc.is_init() && rsrc.is_mutated();
    }

    void eval(SchedulerHandle &info) override {
      met_trace_full();

      // Get shared resources
      const auto &e_rsrc = info.resource(m_info.input_key.first, m_info.input_key.second).read_only<gl::Buffer>();
      auto &i_rsrc       = info.resource(m_info.output_key).writeable<TextureType>();

      // Bind resources to correct buffer/image targets
      e_rsrc.bind_to(gl::BufferTargetType::eShaderStorage,   0);
      i_rsrc.bind_to(gl::TextureTargetType::eImageWriteOnly, 0);
      gl::sync::memory_barrier(gl::BarrierFlags::eShaderStorageBuffer);

      // Dispatch shader, copying buffer into texture object
      gl::dispatch_compute(m_dispatch);
    }
  };
} // namespace met::detail