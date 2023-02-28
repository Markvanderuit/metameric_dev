#pragma once

#include <metameric/core/detail/scheduler_task.hpp>
#include <small_gl/array.hpp>
#include <small_gl/buffer.hpp>
#include <small_gl/dispatch.hpp>
#include <small_gl/program.hpp>

namespace met {
  class ViewportDrawDelaunayTask : public detail::AbstractTask {
    gl::Buffer   m_elem_buffer;
    gl::Array    m_array;
    gl::DrawInfo m_draw_line;
    gl::DrawInfo m_draw_fill;
    gl::Program  m_program;

  public:
    ViewportDrawDelaunayTask(const std::string &);
    void init(detail::TaskInitInfo &) override;
    void eval(detail::TaskEvalInfo &) override;
  };
} // namespace met