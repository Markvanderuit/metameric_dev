#pragma once

#include <metameric/core/scheduler.hpp>
#include <small_gl/program.hpp>
#include <small_gl/buffer.hpp>
#include <small_gl/dispatch.hpp>

namespace met {
  class ValidateSpectralTextureTask : public detail::AbstractTask {
    gl::Program     m_program;
    gl::Buffer      m_uniform_buffer;
    gl::ComputeInfo m_dispatch;

  public:
    ValidateSpectralTextureTask(const std::string &);
    void init(detail::TaskInitInfo &) override;
    void eval(detail::TaskEvalInfo &) override;
  };
} // namespace met