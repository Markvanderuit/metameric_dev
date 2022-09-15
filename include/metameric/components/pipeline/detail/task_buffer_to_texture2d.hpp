#pragma once

#include <metameric/core/scheduler.hpp>
#include <small_gl/dispatch.hpp>
#include <small_gl/program.hpp>
#include <string>
#include <utility>

namespace met {
  template <class TextureTy, class InfoTy = TextureTy::InfoType>
  class BufferToTexture2dTask : public detail::AbstractTask {
    std::string m_inp_task_key;
    std::string m_inp_rsrc_key;
    std::string m_out_rsrc_key;
    InfoTy      m_out_rsrc_info;

    gl::Program     m_program;
    gl::ComputeInfo m_dispatch;

  public:
    BufferToTexture2dTask(const std::string &task_name,
                          const std::string &inp_task_key,
                          const std::string &inp_rsrc_key,
                          InfoTy             out_rsrc_info,
                          const std::string &out_rsrc_key = "out");

    void init(detail::TaskInitInfo &) override;
    void eval(detail::TaskEvalInfo &) override;
  };
} // namespace met