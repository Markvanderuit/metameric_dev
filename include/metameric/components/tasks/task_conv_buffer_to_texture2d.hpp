#pragma once

#include <metameric/core/scheduler.hpp>
#include <small_gl/dispatch.hpp>
#include <small_gl/program.hpp>
#include <string>
#include <utility>

namespace met {
  template <class TextureTy, class InfoTy>
  class ConvBufferToTexture2dTask : public detail::AbstractTask {
    using RefType = std::pair<std::string, std::string>;
    
    std::string m_input_task_key;
    std::string m_input_buffer_key;
    std::string m_output_texture_key;
    InfoTy      m_output_texture_info;

    gl::Program     m_program;
    gl::ComputeInfo m_dispatch;

  public:
    ConvBufferToTexture2dTask(const std::string &task_name,
                        const std::string &input_task_key,
                        const std::string &input_buffer_key,
                        InfoTy             output_texture_info,
                        const std::string &output_texture_key = "texture");

    void init(detail::TaskInitInfo &) override;
    void eval(detail::TaskEvalInfo &) override;
  };
} // namespace met