#pragma once

#include <metameric/core/scheduler.hpp>
#include <small_gl/array.hpp>
#include <small_gl/buffer.hpp>
#include <small_gl/dispatch.hpp>
#include <small_gl/program.hpp>

namespace met {
  class ViewportDrawEmbeddingTask : public detail::TaskNode {
    struct UniformBuffer {
      alignas(64) eig::Matrix4f camera_matrix;
      alignas(8)  eig::Array2u  size_in;  // Nr. of texels to sample from
      alignas(8)  eig::Array2f  viewport_aspect;
      alignas(4)  uint          n_verts;
      alignas(4)  uint          n_quads;
    };
    
    uint m_mapping_i = 1;

    gl::Array         m_array;
    gl::DrawInfo      m_draw;
    gl::Program       m_program;
    gl::Buffer        m_data_buffer;
    gl::Buffer        m_unif_buffer;
    gl::Buffer        m_vert_buffer;
    UniformBuffer    *m_unif_map;
    std::span<AlColr> m_vert_map;

  public:
    void init(SchedulerHandle &) override;
    void eval(SchedulerHandle &) override;
    bool is_active(SchedulerHandle &) override;
  };
} // namespace met