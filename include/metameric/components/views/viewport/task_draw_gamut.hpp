#pragma once

#include <small_gl/array.hpp>
#include <small_gl/buffer.hpp>
#include <small_gl/dispatch.hpp>
#include <small_gl/program.hpp>
#include <metameric/core/detail/scheduler_task.hpp>

namespace met {
  class ViewportDrawGamutTask : public detail::AbstractTask {
    struct UniformBuffer {
      alignas(64) eig::Matrix4f camera_matrix;
      alignas(16) eig::Vector2f camera_aspect;
    };

    // Local uniform buffer to stream shared camera data
    gl::Buffer     m_unif_buffer;
    UniformBuffer *m_unif_map;

    // Local buffers to stream packed vertex data and unaligned element data
    gl::Buffer m_vert_buffer;
    gl::Buffer m_elem_buffer;
    std::span<eig::AlArray3f> m_vert_map;
    std::span<eig::Array3u>   m_elem_map;

    // Local buffer to stream individual opacities/sizes for selection/mouseover
    gl::Buffer        m_vert_size_buffer;
    gl::Buffer        m_elem_opac_buffer;
    std::span<float>  m_vert_size_map;
    std::span<float>  m_elem_opac_map;

    // Graphics draw components
    gl::Array    m_vert_array;
    gl::Array    m_elem_array;
    gl::Program  m_vert_program;
    gl::Program  m_edge_program;
    gl::Program  m_elem_program;
    gl::DrawInfo m_vert_draw;
    gl::DrawInfo m_edge_draw;
    gl::DrawInfo m_elem_draw;

  public:
    ViewportDrawGamutTask(const std::string &);
    void init(detail::TaskInitInfo &) override;
    void eval(detail::TaskEvalInfo &) override;
    void dstr(detail::TaskDstrInfo &) override;
  };
} // namespace met