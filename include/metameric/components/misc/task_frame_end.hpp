#pragma once

#include <metameric/core/scheduler.hpp>
#include <metameric/core/utility.hpp>
#include <metameric/components/views/detail/imgui.hpp>
#include <small_gl/framebuffer.hpp>
#include <small_gl/window.hpp>

namespace met {
  class FrameEndTask : public detail::TaskNode {
    bool m_bind_default_fbo;

  public:
    FrameEndTask(bool bind_default_fbo = true)
    : m_bind_default_fbo(bind_default_fbo) { }

    void eval(SchedulerHandle &info) override {
      met_trace_full();
      
      // Prepare default framebuffer for coming draw
      auto fb = gl::Framebuffer::make_default();
      if (m_bind_default_fbo)
        fb.bind();
      fb.clear(gl::FramebufferType::eColor, eig::Array3f(0).eval());
      fb.clear(gl::FramebufferType::eDepth, 0.f);

      // Handle ImGui events
      ImGui::DrawFrame();

      // Handle window events
      auto &e_window = info.global("window").getw<gl::Window>();
      e_window.swap_buffers();
      e_window.poll_events();

      // Handle tracy events
      met_trace_frame()
    }
  };
} // namespace met