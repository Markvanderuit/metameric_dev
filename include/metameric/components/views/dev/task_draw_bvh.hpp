#pragma once


#include <metameric/core/scheduler.hpp>
#include <small_gl/array.hpp>
#include <small_gl/buffer.hpp>
#include <small_gl/dispatch.hpp>
#include <small_gl/program.hpp>

namespace met {
  class ViewportDrawBVHTask : public detail::TaskNode {
    struct CameraBuffer {
      alignas(64) eig::Matrix4f matrix;
      alignas(16) eig::Vector2f aspect;
    };

    struct UniformBuffer {
      alignas(4) uint node_begin;
      alignas(4) uint node_extent;
    };

    uint                    m_tree_level;

    gl::Buffer              m_tree_buffer;
    gl::Buffer              m_unif_buffer;
    gl::Buffer              m_camr_buffer;
    UniformBuffer          *m_unif_map;
    CameraBuffer           *m_camr_map;
    
    gl::Array               m_array;
    gl::DrawInfo            m_draw;
    gl::Program             m_program;

  public:
    void init(SchedulerHandle &) override;
    void eval(SchedulerHandle &) override;
  };
} // namespace met