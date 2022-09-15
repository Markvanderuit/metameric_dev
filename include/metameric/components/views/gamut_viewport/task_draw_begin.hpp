#pragma once

#include <metameric/core/utility.hpp>
#include <metameric/core/detail/trace.hpp>
#include <metameric/core/detail/scheduler_task.hpp>
#include <small_gl/framebuffer.hpp>
#include <small_gl/renderbuffer.hpp>
#include <small_gl/texture.hpp>

namespace met {
  class DrawBeginTask : public detail::AbstractTask {
    using Colorbuffer = gl::Renderbuffer<float, 4, gl::RenderbufferType::eMultisample>;
    using Depthbuffer = gl::Renderbuffer<gl::DepthComponent, 1, gl::RenderbufferType::eMultisample>;

    std::string m_parent;

    // Multisampled framebuffer attachments
    Colorbuffer m_color_buffer_ms;
    Depthbuffer m_depth_buffer_ms;

  public:
    DrawBeginTask(const std::string &name, const std::string &parent)
    : detail::AbstractTask(name, true),
      m_parent(parent) { }

    void init(detail::TaskInitInfo &info) override {
      met_trace_full();
      
      // Share uninitialized framebuffer objects; initialized during eval()
      info.insert_resource("frame_buffer",    gl::Framebuffer());
      info.insert_resource("frame_buffer_ms", gl::Framebuffer());
    }

    void eval(detail::TaskEvalInfo &info) override {
      met_trace_full();
    
      // Get shared resources 
      auto &e_target_texture  = info.get_resource<gl::Texture2d3f>(m_parent, "draw_texture");
      auto &i_frame_buffer    = info.get_resource<gl::Framebuffer>("frame_buffer");
      auto &i_frame_buffer_ms = info.get_resource<gl::Framebuffer>("frame_buffer_ms");

      // (Re-)create framebuffers and renderbuffers if the viewport has resized
      if (!i_frame_buffer.is_init() || (e_target_texture.size() != m_color_buffer_ms.size()).any()) {
        m_color_buffer_ms = {{ .size = e_target_texture.size() }};
        m_depth_buffer_ms = {{ .size = e_target_texture.size() }};
        i_frame_buffer_ms = {{ .type = gl::FramebufferType::eColor, .attachment = &m_color_buffer_ms },
                             { .type = gl::FramebufferType::eDepth, .attachment = &m_depth_buffer_ms }};
        i_frame_buffer    = {{ .type = gl::FramebufferType::eColor, .attachment = &e_target_texture }};
      }

      // Clear framebuffer target for next subtasks
      i_frame_buffer_ms.clear(gl::FramebufferType::eColor, eig::Array4f(0.f));
      i_frame_buffer_ms.clear(gl::FramebufferType::eDepth, 1.f);
      i_frame_buffer_ms.bind();

      // Specify viewport for next subtasks
      gl::state::set_viewport(e_target_texture.size());
    }
  };
} // namespace  met
