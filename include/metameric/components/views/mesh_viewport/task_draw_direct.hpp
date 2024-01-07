#pragma once

#include <metameric/core/scheduler.hpp>
#include <small_gl/buffer.hpp>
#include <small_gl/program.hpp>

namespace met {
  class MeshViewportDrawDirectTask : public detail::TaskNode {
    struct UnifLayout {
      eig::Matrix4f trf;
      eig::Vector2u viewport_size;
    };

    struct SamplerLayout {
      uint iter;
      uint n_iters_per_dispatch;
    };

    gl::Buffer     m_unif_buffer;
    gl::Buffer     m_sampler_buffer;
    gl::Buffer     m_state_buffer;
    gl::Program    m_program;
    UnifLayout    *m_unif_buffer_map;
    SamplerLayout *m_sampler_buffer_map;

    uint m_iter;
    
  public:
    bool is_active(SchedulerHandle &info) override;
    void init(SchedulerHandle &info)      override;
    void eval(SchedulerHandle &info)      override;
  };
} // namespace met