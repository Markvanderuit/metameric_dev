#pragma once

#include <metameric/core/scheduler.hpp>
#include <small_gl/buffer.hpp>
#include <small_gl/dispatch.hpp>
#include <small_gl/program.hpp>

namespace met {
  class GenSpectralTextureTask : public detail::AbstractTask {
    gl::ComputeInfo m_generate_dispatch;
    gl::Program     m_generate_program;

  public:
    GenSpectralTextureTask(const std::string &name);
    void init(detail::TaskInitInfo &) override;
    void eval(detail::TaskEvalInfo &) override;
  };
} // namespace met