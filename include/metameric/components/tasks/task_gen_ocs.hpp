#pragma once

#include <metameric/core/scheduler.hpp>
#include <small_gl/dispatch.hpp>
#include <small_gl/program.hpp>

namespace met {
  class GenOCSTask : public detail::AbstractTask {
    gl::Program     m_program;    
    gl::ComputeInfo m_dispatch;    
    bool            m_stale;

  public:
    GenOCSTask(const std::string &name);
    void init(detail::TaskInitInfo &) override;
    void eval(detail::TaskEvalInfo &) override;
  };
} // namespace met