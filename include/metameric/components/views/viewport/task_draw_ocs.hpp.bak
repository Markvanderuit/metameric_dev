#pragma once

#include <metameric/core/detail/scheduler_task.hpp>
#include <small_gl/array.hpp>
#include <small_gl/buffer.hpp>
#include <small_gl/dispatch.hpp>
#include <small_gl/program.hpp>

namespace met {
  class ViewportDrawOCSTask : public detail::AbstractTask {
    gl::Buffer   m_hull_vertices;
    gl::Buffer   m_hull_elements;
    gl::Array    m_hull_array;
    gl::DrawInfo m_hull_dispatch;
    gl::Buffer   m_hull_wf_vertices;
    gl::Buffer   m_hull_wf_elements;
    gl::Array    m_hull_wf_array;
    gl::DrawInfo m_hull_wf_dispatch;
    gl::Program  m_program;

  public:
    ViewportDrawOCSTask(const std::string &name);
    void init(detail::TaskInitInfo &) override;
    void eval(detail::TaskEvalInfo &) override;
  };
} // namespace met