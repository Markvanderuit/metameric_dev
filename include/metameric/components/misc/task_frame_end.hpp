#pragma once

#include <metameric/core/scheduler.hpp>
#include <metameric/components/views/detail/imgui.hpp>
#include <small_gl/framebuffer.hpp>
#include <small_gl/window.hpp>

namespace met {
  struct FrameEndTask : public detail::AbstractTask {
    FrameEndTask(const std::string &name)
    : detail::AbstractTask(name) { }

    void eval(detail::TaskEvalInfo &info) override {
      auto fb = gl::Framebuffer::make_default();
      fb.bind();
      fb.clear(gl::FramebufferType::eColor, eig::Array3f(0));
      fb.clear(gl::FramebufferType::eDepth, 0.f);

      ImGui::DrawFrame();

      auto &e_window = info.get_resource<gl::Window>(global_key, "window");
      e_window.swap_buffers();
      e_window.poll_events();
    }
  };
} // namespace met