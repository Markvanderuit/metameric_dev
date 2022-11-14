#pragma once

#include <metameric/core/utility.hpp>
#include <metameric/core/detail/trace.hpp>
#include <metameric/core/detail/scheduler_task.hpp>
#include <small_gl/framebuffer.hpp>
#include <small_gl/renderbuffer.hpp>
#include <small_gl/texture.hpp>

namespace met {
  class ViewportDrawBeginTask : public detail::AbstractTask {
    using Colorbuffer = gl::Renderbuffer<float, 4, gl::RenderbufferType::eMultisample>;
    using Depthbuffer = gl::Renderbuffer<gl::DepthComponent, 1, gl::RenderbufferType::eMultisample>;

    // Framebuffer attachments
    Colorbuffer m_color_buffer_msaa;
    Depthbuffer m_depth_buffer_msaa;
    
  public:
    ViewportDrawBeginTask(const std::string &name)
    : detail::AbstractTask(name, true) { }

    void init(detail::TaskInitInfo &info) override {
      met_trace_full();
    
      // Share uninitialized framebuffer objects; initialized during eval()
      info.insert_resource("frame_buffer", gl::Framebuffer());
      info.insert_resource("frame_buffer_msaa", gl::Framebuffer());
    }

    void eval(detail::TaskEvalInfo &info) override {
      met_trace_full();
    
      // Get shared resources 
      auto &e_draw_texture      = info.get_resource<gl::Texture2d3f>("viewport_begin", "draw_texture");
      auto &i_frame_buffer      = info.get_resource<gl::Framebuffer>("frame_buffer");
      auto &i_frame_buffer_msaa = info.get_resource<gl::Framebuffer>("frame_buffer_msaa");

      // (Re-)create framebuffers and renderbuffers if the viewport has resized
      if (!i_frame_buffer.is_init() || (e_draw_texture.size() != m_color_buffer_msaa.size()).any()) {
        m_color_buffer_msaa = {{ .size = e_draw_texture.size().max(1) }};
        m_depth_buffer_msaa = {{ .size = e_draw_texture.size().max(1) }};
        i_frame_buffer_msaa = {{ .type = gl::FramebufferType::eColor, .attachment = &m_color_buffer_msaa },
                               { .type = gl::FramebufferType::eDepth, .attachment = &m_depth_buffer_msaa }};
        i_frame_buffer      = {{ .type = gl::FramebufferType::eColor, .attachment = &e_draw_texture }};
      }

      // Clear framebuffer target for next subtasks
      i_frame_buffer_msaa.clear(gl::FramebufferType::eColor, eig::Array4f(0.f));
      i_frame_buffer_msaa.clear(gl::FramebufferType::eDepth, 1.f);
      i_frame_buffer_msaa.bind();

      // Specify viewport for next subtasks
      gl::state::set_viewport(e_draw_texture.size());    }
  };
} // namespace met
