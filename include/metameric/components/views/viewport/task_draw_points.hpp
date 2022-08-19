#pragma once

#include <metameric/core/detail/scheduler_task.hpp>
#include <small_gl/array.hpp>
#include <small_gl/dispatch.hpp>
#include <small_gl/program.hpp>

namespace met {
  class ViewportDrawPointsTask : public detail::AbstractTask {
    gl::Array    m_array;
    gl::Program  m_program;
    gl::DrawInfo m_draw;
    float        m_psize = 1.f;
    bool         m_stale;

  public:
    ViewportDrawPointsTask(const std::string &name);
    void init(detail::TaskInitInfo &) override;
    void eval(detail::TaskEvalInfo &) override;
  };
} // namespace met