#pragma once

#include <metameric/core/scheduler.hpp>
#include <small_gl/array.hpp>
#include <small_gl/buffer.hpp>
#include <small_gl/dispatch.hpp>
#include <small_gl/program.hpp>

namespace met {
  class MeshViewportDrawUpliftedTask : public detail::TaskNode {
    struct UnifCameraLayout {
     eig::Matrix4f camera_matrix;
     float wvl;
    };

    UnifCameraLayout *m_unif_camera_buffer_map;
    gl::Buffer        m_unif_camera_buffer;
    gl::Program       m_program;
    gl::MultiDrawInfo m_draw;
    
  public:
    bool is_active(SchedulerHandle &info) override;
    void init(SchedulerHandle &info)      override;
    void eval(SchedulerHandle &info)      override;
  };
} // namespace met