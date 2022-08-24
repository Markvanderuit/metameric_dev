#pragma once

#include <metameric/core/detail/scheduler_task.hpp>
#include <small_gl/array.hpp>
#include <small_gl/dispatch.hpp>
#include <small_gl/program.hpp>

namespace met {
  class ViewportDrawOCSTask : public detail::AbstractTask {
    gl::Array    m_array_points;
    gl::Array    m_array_hull;
    gl::Program  m_program;
    gl::DrawInfo m_draw_points;
    gl::DrawInfo m_draw_hull;
    float        m_psize = 1.f;
    bool         m_stale;

  public:
    ViewportDrawOCSTask(const std::string &name);
    void init(detail::TaskInitInfo &) override;
    void eval(detail::TaskEvalInfo &) override;
  };
} // namespace met