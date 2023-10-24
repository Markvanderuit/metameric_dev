#pragma once

#include <metameric/core/scheduler.hpp>
#include <small_gl/array.hpp>
#include <small_gl/buffer.hpp>
#include <small_gl/dispatch.hpp>
#include <small_gl/program.hpp>

namespace met {
  class ViewportDrawColorSystemSolid : public detail::TaskNode {
    struct UniformBuffer {
      alignas(64) eig::Matrix4f model_matrix;
      alignas(64) eig::Matrix4f camera_matrix;
      
      alignas(4)  float alpha;
      
      alignas(16) Colr          color;
      alignas(4)  bool override_color;
    };
    
    gl::Buffer     m_unif_buffer_line;
    gl::Buffer     m_unif_buffer_fill;
    UniformBuffer *m_unif_map_line;
    UniformBuffer *m_unif_map_fill;
    gl::Buffer     m_vert_buffer;
    gl::Buffer     m_elem_buffer;
    gl::Array      m_array;
    gl::DrawInfo   m_draw_line;
    gl::DrawInfo   m_draw_fill;
    gl::Program    m_program;

  public:
    void init(SchedulerHandle &) override;
    void eval(SchedulerHandle &) override;
  };
} // namespace met