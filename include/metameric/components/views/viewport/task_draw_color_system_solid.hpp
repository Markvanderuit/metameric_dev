#pragma once

#include <metameric/core/scheduler.hpp>
#include <small_gl/array.hpp>
#include <small_gl/buffer.hpp>
#include <small_gl/dispatch.hpp>
#include <small_gl/program.hpp>

namespace met {
  class ViewportDrawColorSystemSolid : public detail::TaskNode {
    gl::Buffer   m_vert_buffer;
    gl::Buffer   m_elem_buffer;
    gl::Array    m_array;
    gl::DrawInfo m_draw_line;
    gl::DrawInfo m_draw_fill;
    gl::Program  m_program;

  public:
    void init(SchedulerHandle &) override;
    void eval(SchedulerHandle &) override;
  };
} // namespace met