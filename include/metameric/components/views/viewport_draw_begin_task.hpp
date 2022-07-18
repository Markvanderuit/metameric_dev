#pragma once

#include <small_gl/framebuffer.hpp>
#include <small_gl/renderbuffer.hpp>
#include <small_gl/texture.hpp>
#include <metameric/core/detail/scheduler_task.hpp>

namespace met {
  namespace detail {
    using Colorbuffer = gl::Renderbuffer<float, 3, gl::RenderbufferType::eMultisample>;
    using Depthbuffer = gl::Renderbuffer<gl::DepthComponent, 1, gl::RenderbufferType::eMultisample>;
  } // namespace detail

  class ViewportDrawBeginTask : public detail::AbstractTask {
    // Framebuffer attachments
    detail::Colorbuffer m_viewport_cbuffer_msaa;
    detail::Depthbuffer m_viewport_dbuffer_msaa;
    
  public:
    ViewportDrawBeginTask(const std::string &name)
    : detail::AbstractTask(name) { }

    void init(detail::TaskInitInfo &info) override {
      // Share uninitialized framebuffer objects; initialized during eval()
      info.insert_resource("viewport_fbuffer", gl::Framebuffer());
      info.insert_resource("viewport_fbuffer_msaa", gl::Framebuffer());
    }

    void eval(detail::TaskEvalInfo &info) override {
      // Get shared resources 
      auto &e_viewport_texture      = info.get_resource<gl::Texture2d3f>("viewport", "viewport_texture");
      auto &i_viewport_fbuffer      = info.get_resource<gl::Framebuffer>("viewport_fbuffer");
      auto &i_viewport_fbuffer_msaa = info.get_resource<gl::Framebuffer>("viewport_fbuffer_msaa");

      // (Re-)create framebuffers and renderbuffers if the viewport has resized
      if (!i_viewport_fbuffer.is_init() || e_viewport_texture.size() != m_viewport_cbuffer_msaa.size()) {
        m_viewport_cbuffer_msaa = {{ .size = e_viewport_texture.size() }};
        m_viewport_dbuffer_msaa = {{ .size = e_viewport_texture.size() }};
        i_viewport_fbuffer_msaa = {{ .type = gl::FramebufferType::eColor, .attachment = &m_viewport_cbuffer_msaa },
                                   { .type = gl::FramebufferType::eDepth, .attachment = &m_viewport_dbuffer_msaa }};
        i_viewport_fbuffer      = {{ .type = gl::FramebufferType::eColor, .attachment = &e_viewport_texture }};
      }

      // Clear framebuffer targets
      i_viewport_fbuffer_msaa.clear(gl::FramebufferType::eColor, glm::vec3(0.f));
      i_viewport_fbuffer_msaa.clear(gl::FramebufferType::eDepth, 1.f);
    }
  };
} // namespace met
