#pragma once

#include <metameric/core/scheduler.hpp>
#include <small_gl/array.hpp>
#include <small_gl/buffer.hpp>
#include <small_gl/dispatch.hpp>
#include <small_gl/program.hpp>

namespace met {
  class ViewportDrawCubeTask : public detail::TaskNode {
    struct UniformBuffer {
      alignas(64) eig::Matrix4f model_matrix;
      alignas(64) eig::Matrix4f camera_matrix;
      alignas(16) eig::Vector4f color_value;
    };
    
    gl::Buffer   m_vert_buffer;
    gl::Buffer   m_elem_buffer;
    gl::Buffer   m_unif_buffer;
    UniformBuffer *m_unif_map;
    gl::Array    m_array;
    gl::DrawInfo m_draw;
    gl::Program  m_program;

  public:
    void init(SchedulerHandle &) override;
    void eval(SchedulerHandle &) override;
  };
} // namespace met