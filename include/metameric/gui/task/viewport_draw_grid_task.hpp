#pragma once

#include <small_gl/array.hpp>
#include <small_gl/buffer.hpp>
#include <small_gl/dispatch.hpp>
#include <small_gl/program.hpp>
#include <metameric/gui/detail/linear_scheduler/task.hpp>

namespace met {
  class ViewportDrawGridTask : public detail::AbstractTask {
    // Draw components
    gl::Buffer   m_grid_vertex_buffer;
    gl::Array    m_grid_array;
    gl::Program  m_grid_program;
    gl::DrawInfo m_grid_draw;
    float        m_grid_psize = 1.f;
    
  public:
    ViewportDrawGridTask(const std::string &name);
    void init(detail::TaskInitInfo &) override;
    void eval(detail::TaskEvalInfo &) override;
  };
} // namespace met