#pragma once

#include <metameric/core/scheduler.hpp>
#include <small_gl/array.hpp>
#include <small_gl/framebuffer.hpp>
#include <small_gl/renderbuffer.hpp>
#include <small_gl/program.hpp>

namespace met {
  class MeshViewportDrawOverlayTask : public detail::TaskNode {
    using Depthbuffer = gl::Renderbuffer<gl::DepthComponent, 1>;

    gl::Program     m_program;
    gl::Array       m_vao;
    gl::Framebuffer m_fbo;
    Depthbuffer     m_dbo;

    void eval_draw_constraints(SchedulerHandle &info);
    void eval_draw_path_queries(SchedulerHandle &info);

  public:
    void init(SchedulerHandle &info)      override;
    void eval(SchedulerHandle &info)      override;
  };
} // namespace met