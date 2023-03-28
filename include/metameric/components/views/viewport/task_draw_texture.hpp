#pragma once

#include <metameric/core/scheduler.hpp>
#include <small_gl/array.hpp>
#include <small_gl/buffer.hpp>
#include <small_gl/dispatch.hpp>
#include <small_gl/program.hpp>

namespace met {
  class ViewportDrawTextureTask : public detail::TaskNode {
    struct UniformBuffer {
      alignas(64) eig::Matrix4f camera_matrix;
      alignas(16) eig::Vector2f camera_aspect;
    };
    
    gl::Array      m_array;
    gl::DrawInfo   m_draw;
    gl::Program    m_program;
    gl::Buffer     m_data_buffer;
    gl::Buffer     m_uniform_buffer;
    UniformBuffer *m_uniform_map;

  public:
    void init(SchedulerHandle &) override;
    void eval(SchedulerHandle &) override;
  };
} // namespace met
