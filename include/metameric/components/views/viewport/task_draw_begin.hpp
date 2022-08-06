#pragma once

#include <small_gl/framebuffer.hpp>
#include <small_gl/renderbuffer.hpp>
#include <small_gl/texture.hpp>
#include <metameric/core/detail/scheduler_task.hpp>

namespace met {
  class ViewportDrawBeginTask : public detail::AbstractTask {
    using Colorbuffer = gl::Renderbuffer<float, 3, gl::RenderbufferType::eMultisample>;
    using Depthbuffer = gl::Renderbuffer<gl::DepthComponent, 1, gl::RenderbufferType::eMultisample>;

    // Framebuffer attachments
    Colorbuffer m_color_buffer_msaa;
    Depthbuffer m_depth_buffer_msaa;
    
  public:
    ViewportDrawBeginTask(const std::string &name)
    : detail::AbstractTask(name) { }

    void init(detail::TaskInitInfo &info) override {
      // Share uninitialized framebuffer objects; initialized during eval()
      info.insert_resource("frame_buffer", gl::Framebuffer());
      info.insert_resource("frame_buffer_msaa", gl::Framebuffer());
    }

    void eval(detail::TaskEvalInfo &info) override {
      // Get shared resources 
      auto &e_draw_texture      = info.get_resource<gl::Texture2d3f>("viewport", "draw_texture");
      auto &i_frame_buffer      = info.get_resource<gl::Framebuffer>("frame_buffer");
      auto &i_frame_buffer_msaa = info.get_resource<gl::Framebuffer>("frame_buffer_msaa");

      // (Re-)create framebuffers and renderbuffers if the viewport has resized
      if (!i_frame_buffer.is_init() || (e_draw_texture.size() != m_color_buffer_msaa.size()).any()) {
        m_color_buffer_msaa = {{ .size = e_draw_texture.size() }};
        m_depth_buffer_msaa = {{ .size = e_draw_texture.size() }};
        i_frame_buffer_msaa = {{ .type = gl::FramebufferType::eColor, .attachment = &m_color_buffer_msaa },
                               { .type = gl::FramebufferType::eDepth, .attachment = &m_depth_buffer_msaa }};
        i_frame_buffer      = {{ .type = gl::FramebufferType::eColor, .attachment = &e_draw_texture }};
      }

      // Clear framebuffer targets
      i_frame_buffer_msaa.clear(gl::FramebufferType::eColor, eig::Array3f(0));
      i_frame_buffer_msaa.clear(gl::FramebufferType::eDepth, 1.f);
    }
  };
} // namespace met
