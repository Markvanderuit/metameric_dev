#pragma once

#include <metameric/core/scheduler.hpp>
#include <small_gl/buffer.hpp>
#include <small_gl/program.hpp>

namespace met {
  class MeshViewportDrawRaytraceTask : public detail::TaskNode {
    struct UnifLayout {
      alignas(16) eig::Matrix4f view_inv;  // inverse(view);
      alignas(8)  eig::Vector2u view_size; // width, height of image
      alignas(4)  float         fovy_tan;  // tan(fov_y * .5f);
      alignas(4)  float         aspect;    // vec2(aspect, 1);
    };

    gl::Buffer  m_buffer_sampler_state;
    gl::Buffer  m_buffer_work;
    gl::Buffer  m_buffer_work_head;
    gl::Buffer  m_buffer_unif;
    UnifLayout *m_buffer_unif_map;

    gl::Program m_program_ray_init;
    gl::Program m_program_ray_isct;
    gl::Program m_program_ray_draw;
    
  public:
    bool is_active(SchedulerHandle &info) override;
    void init(SchedulerHandle &info)      override;
    void eval(SchedulerHandle &info)      override;
  };
} // namespace met