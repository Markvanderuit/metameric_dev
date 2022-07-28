#pragma once

#include <small_gl/array.hpp>
#include <small_gl/buffer.hpp>
#include <small_gl/dispatch.hpp>
#include <small_gl/program.hpp>
#include <metameric/core/detail/scheduler_task.hpp>

namespace met {
  class ViewportDrawGridTask : public detail::AbstractTask {
    // Draw components
    gl::Buffer   m_vertex_buffer;
    gl::Array    m_vertex_array;
    gl::Program  m_program;
    gl::DrawInfo m_draw;
    float        m_psize = 1.f;
    
  public:
    ViewportDrawGridTask(const std::string &name);
    void init(detail::TaskInitInfo &) override;
    void eval(detail::TaskEvalInfo &) override;
  };
} // namespace met