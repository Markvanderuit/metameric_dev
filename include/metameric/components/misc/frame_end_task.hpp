#pragma once

#include <metameric/core/scheduler.hpp>
#include <metameric/components/views/detail/imgui.hpp>
#include <small_gl/framebuffer.hpp>

namespace met {
  struct FrameEndTask : public detail::AbstractTask {
    FrameEndTask(const std::string &name)
    : detail::AbstractTask(name) { }

    void eval(detail::TaskEvalInfo &info) override {
      auto fb = gl::Framebuffer::make_default();
      fb.bind();
      fb.clear<glm::vec3>(gl::FramebufferType::eColor);
      fb.clear<float>(gl::FramebufferType::eDepth);

      ImGui::DrawFrame();

      auto &e_window = info.get_resource<gl::Window>("global", "window");
      e_window.swap_buffers();
      e_window.poll_events();
    }
  };
} // namespace met