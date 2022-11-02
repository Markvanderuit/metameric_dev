#pragma once

#include <small_gl/array.hpp>
#include <small_gl/buffer.hpp>
#include <small_gl/dispatch.hpp>
#include <small_gl/program.hpp>
#include <metameric/core/detail/scheduler_task.hpp>

namespace met {
  class ViewportDrawVerticesTask : public detail::AbstractTask {
    gl::Buffer   m_vert_buffer;
    gl::Buffer   m_elem_buffer;
    gl::Buffer   m_size_buffer;
    gl::Array    m_array;
    gl::DrawInfo m_draw;
    gl::Program  m_program;

  public:
    ViewportDrawVerticesTask(const std::string &);
    void init(detail::TaskInitInfo &) override;
    void dstr(detail::TaskDstrInfo &) override;
    void eval(detail::TaskEvalInfo &) override;
  };
} // namespace met
