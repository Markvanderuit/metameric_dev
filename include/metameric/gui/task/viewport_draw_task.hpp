#pragma once

#include <small_gl/array.hpp>
#include <small_gl/buffer.hpp>
#include <small_gl/dispatch.hpp>
#include <small_gl/program.hpp>
#include <metameric/gui/detail/linear_scheduler/task.hpp>

namespace met {
  class ViewportDrawTask : public detail::AbstractTask {
    // Cube draw components
    gl::Buffer   m_cube_vertex_buffer;
    gl::Buffer   m_cube_elem_buffer;
    gl::Array    m_cube_array;
    gl::DrawInfo m_cube_draw;
    gl::Program  m_cube_program; 
    float        m_cube_lwidth = 1.f;

    // Gamut draw components
    gl::Buffer   m_gamut_elem_buffer;
    gl::Array    m_gamut_array;
    gl::DrawInfo m_gamut_draw;
    gl::Program  m_gamut_program;
    float        m_gamut_lwidth = 4.f;
    
    // Spectral pointset draw components
    gl::Buffer   m_data_points_buffer; 
    gl::Array    m_data_points_array;
    gl::DrawInfo m_data_points_draw;
    gl::Program  m_data_points_program;
    float        m_data_points_psize = 1.f;
    float        m_data_points_popaq = 1.f;

    // Texture pointset draw components
    gl::Buffer   m_texture_points_buffer;
    gl::Array    m_texture_points_array;
    gl::DrawInfo m_texture_points_draw;
    gl::Program  m_texture_points_program;
    float        m_texture_points_psize = 4.f;
    float        m_texture_points_popaq = 4.f;

  public:
    ViewportDrawTask(const std::string &name);
    void init(detail::TaskInitInfo &) override;
    void eval(detail::TaskEvalInfo &) override;
  };
} // namespace met