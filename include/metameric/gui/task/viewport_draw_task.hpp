#pragma once

#include <small_gl/array.hpp>
#include <small_gl/buffer.hpp>
#include <small_gl/framebuffer.hpp>
#include <small_gl/program.hpp>
#include <small_gl/renderbuffer.hpp>
#include <metameric/gui/detail/arcball.hpp>
#include <metameric/gui/detail/linear_scheduler/task.hpp>

namespace met {
  class ViewportDrawTask : public detail::AbstractTask {
    // Gamut draw components
    gl::Buffer   m_gamut_elem_buffer;
    gl::Array    m_gamut_array;
    gl::DrawInfo m_gamut_draw;
    gl::Program  m_gamut_program;
    float        m_gamut_lwidth = 1.f;
    
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

    // Framebuffers and attachments
    gl::Renderbuffer<float, 3, gl::RenderbufferType::eMultisample>
                     m_rbuffer_msaa;
    gl::Renderbuffer<gl::DepthComponent, 1, gl::RenderbufferType::eMultisample>
                     m_dbuffer_msaa;
    gl::Framebuffer  m_fbuffer_msaa;
    gl::Framebuffer  m_fbuffer;
    glm::vec3        m_fbuffer_clear_value;

  public:
    ViewportDrawTask(const std::string &name);
    void init(detail::TaskInitInfo &) override;
    void eval(detail::TaskEvalInfo &) override;
  };
} // namespace met