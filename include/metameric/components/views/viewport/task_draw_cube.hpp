#pragma once

#include <small_gl/array.hpp>
#include <small_gl/buffer.hpp>
#include <small_gl/dispatch.hpp>
#include <small_gl/program.hpp>
#include <metameric/core/detail/scheduler_task.hpp>

namespace met {
  class ViewportDrawCubeTask : public detail::AbstractTask {
    gl::Buffer   m_vert_buffer;
    gl::Buffer   m_elem_buffer;
    gl::Array    m_array;
    gl::DrawInfo m_draw;
    gl::Program  m_program;

  public:
    ViewportDrawCubeTask(const std::string &);
    void init(detail::TaskInfo &) override;
    void eval(detail::TaskInfo &) override;
  };
} // namespace met