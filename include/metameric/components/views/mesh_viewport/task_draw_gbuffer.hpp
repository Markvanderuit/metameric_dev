#pragma once

#include <metameric/core/scheduler.hpp>
#include <small_gl/buffer.hpp>
#include <small_gl/framebuffer.hpp>
#include <small_gl/renderbuffer.hpp>
#include <small_gl/dispatch.hpp>
#include <small_gl/program.hpp>

namespace met {
  class MeshViewportDrawGBufferTask : public detail::TaskNode {
    using Depthbuffer = gl::Renderbuffer<gl::DepthComponent, 1>;

    struct UnifLayout {
     eig::Matrix4f trf;
     eig::Matrix4f trf_inv;
     eig::Array2f  viewport_size;
     float         z_near;
     float         z_far;
    };
    
    UnifLayout       *m_unif_buffer_map;
    gl::Buffer        m_unif_buffer;
    Depthbuffer       m_fbo_depth;
    gl::Framebuffer   m_fbo;
    gl::Program       m_program;
    gl::MultiDrawInfo m_draw;
    
  public:
    bool is_active(SchedulerHandle &info) override;
    void init(SchedulerHandle &info)      override;
    void eval(SchedulerHandle &info)      override;
  };
} // namespace met