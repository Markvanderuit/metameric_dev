#pragma once

#include <metameric/core/scheduler.hpp>
#include <small_gl/buffer.hpp>
#include <small_gl/program.hpp>

namespace met {
  class MeshViewportDrawCombineTask : public detail::TaskNode {
    struct UnifLayout {
      eig::Vector2u viewport_size;
    };

    gl::Buffer   m_unif_buffer;
    UnifLayout  *m_unif_buffer_map;
    gl::Program  m_program;
    
  public:
    void init(SchedulerHandle &info)      override;
    void eval(SchedulerHandle &info)      override;
  };
} // namespace met