#pragma once

#include <small_gl/array.hpp>
#include <small_gl/buffer.hpp>
#include <small_gl/dispatch.hpp>
#include <small_gl/program.hpp>
#include <metameric/core/detail/scheduler_task.hpp>

namespace met {
  class ViewportDrawGamutTask : public detail::AbstractTask {
    // Gamut draw components
    gl::Buffer   m_gamut_elem_buffer;
    gl::Array    m_gamut_array;
    gl::DrawInfo m_gamut_draw;
    gl::Program  m_gamut_program;

  public:
    ViewportDrawGamutTask(const std::string &);
    void init(detail::TaskInitInfo &) override;
    void eval(detail::TaskEvalInfo &) override;
  };
} // namespace met