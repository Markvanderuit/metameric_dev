#pragma once

#include <small_gl/array.hpp>
#include <small_gl/buffer.hpp>
#include <small_gl/dispatch.hpp>
#include <small_gl/program.hpp>
#include <metameric/core/detail/scheduler_task.hpp>

namespace met {
  class ViewportDrawTextureTask : public detail::AbstractTask {
    gl::Array    m_array;
    gl::DrawInfo m_draw;
    gl::Program  m_program;

  public:
    ViewportDrawTextureTask(const std::string &);
    void init(detail::TaskInitInfo &) override;
    void eval(detail::TaskEvalInfo &) override;
  };
} // namespace met
