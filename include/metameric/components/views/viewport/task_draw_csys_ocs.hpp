#pragma once

#include <metameric/core/detail/scheduler_task.hpp>
#include <small_gl/array.hpp>
#include <small_gl/buffer.hpp>
#include <small_gl/dispatch.hpp>
#include <small_gl/program.hpp>

namespace met {
  class ViewportDrawCSysOCSTask : public detail::AbstractTask {
    gl::Buffer   m_vert_buffer;
    gl::Buffer   m_elem_buffer;
    gl::Array    m_array;
    gl::DrawInfo m_draw;
    gl::Program  m_program;

  public:
    ViewportDrawCSysOCSTask(const std::string &);
    void init(detail::TaskInitInfo &) override;
    void eval(detail::TaskEvalInfo &) override;
  };
} // namespace met