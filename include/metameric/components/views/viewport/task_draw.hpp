#pragma once

#include <small_gl/array.hpp>
#include <small_gl/buffer.hpp>
#include <small_gl/dispatch.hpp>
#include <small_gl/program.hpp>
#include <metameric/core/detail/scheduler_task.hpp>

namespace met {
  class ViewportDrawTask : public detail::AbstractTask {
    // Gamut draw components
    gl::Buffer   m_gamut_elem_buffer;
    gl::Array    m_gamut_array;
    gl::DrawInfo m_gamut_draw;
    gl::Program  m_gamut_program;
    float        m_gamut_lwidth = 1.f;

    // Texture pointset draw components
    gl::Array    m_texture_points_array;
    gl::DrawInfo m_texture_points_draw;
    gl::Program  m_texture_points_program;
    float        m_texture_points_psize = 1.f;

  public:
    ViewportDrawTask(const std::string &);
    void init(detail::TaskInitInfo &) override;
    void eval(detail::TaskEvalInfo &) override;
  };
} // namespace met