#pragma once

#include <metameric/core/scheduler.hpp>
#include <small_gl/buffer.hpp>
#include <small_gl/dispatch.hpp>
#include <small_gl/program.hpp>

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

    InfoType        m_info;
    gl::ComputeInfo m_dispatch;
    gl::Program     m_program;
    gl::Buffer      m_uniform;

  public:
    TextureFromBufferTask(InfoType info)
    : m_info(info) { }
    
    void init(SchedulerHandle &) override;
    void eval(SchedulerHandle &) override;
    bool is_active(SchedulerHandle &) override;
  };
} // namespace met::detail