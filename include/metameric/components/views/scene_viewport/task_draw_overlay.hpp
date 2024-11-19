#pragma once

#include <metameric/core/scheduler.hpp>
#include <small_gl/array.hpp>
#include <small_gl/framebuffer.hpp>
#include <small_gl/renderbuffer.hpp>
#include <small_gl/program.hpp>

namespace met {
  class ViewportDrawOverlayTask : public detail::TaskNode {
    using Depthbuffer = gl::Renderbuffer<gl::DepthComponent, 1>;

    std::string     m_program_key; 
    gl::Array       m_vao;
    gl::Framebuffer m_fbo;
    Depthbuffer     m_dbo;

    void eval_draw_constraints(SchedulerHandle &info);
    void eval_draw_path_queries(SchedulerHandle &info);
    void eval_draw_frustrum(SchedulerHandle &info);
    void eval_draw_info(SchedulerHandle &info);

  public:
    bool is_active(SchedulerHandle &info) override;
    void init(SchedulerHandle &info)      override;
    void eval(SchedulerHandle &info)      override;
  };
} // namespace met