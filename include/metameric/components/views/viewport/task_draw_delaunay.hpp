#pragma once

#include <metameric/core/scheduler.hpp>
#include <small_gl/array.hpp>
#include <small_gl/buffer.hpp>
#include <small_gl/dispatch.hpp>
#include <small_gl/program.hpp>

namespace met {
  class ViewportDrawDelaunayTask : public detail::TaskBase {
    struct UniformBuffer {
      alignas(64) eig::Matrix4f camera_matrix;
      alignas(16) eig::Vector2f camera_aspect;
    };

    gl::Buffer              m_size_buffer;
    gl::Buffer              m_elem_buffer;
    gl::Buffer              m_unif_buffer;
    std::span<float>        m_size_map;
    std::span<eig::Array3u> m_elem_map;
    UniformBuffer          *m_unif_map;

    gl::Array               m_vert_array;
    gl::DrawInfo            m_vert_draw;
    gl::Program             m_vert_program;
    gl::Array               m_elem_array;
    gl::DrawInfo            m_elem_draw;
    gl::Program             m_elem_program;

  public:
    void init(SchedulerHandle &) override;
    void eval(SchedulerHandle &) override;
  };
} // namespace met