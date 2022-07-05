#pragma once

#include <small_gl/framebuffer.hpp>
#include <small_gl/texture.hpp>
#include <metameric/gui/detail/linear_scheduler/task.hpp>

namespace met {
  struct ViewportDrawEndTask : public detail::AbstractTask {
    ViewportDrawEndTask(const std::string &name)
    : detail::AbstractTask(name) { }

    void init(detail::TaskInitInfo &info) override { }

    void eval(detail::TaskEvalInfo &info) override {
      // Get shared resources 
      auto &e_viewport_texture      = info.get_resource<gl::Texture2d3f>("viewport", "viewport_texture");
      auto &e_viewport_fbuffer      = info.get_resource<gl::Framebuffer>("viewport_draw_begin", "viewport_fbuffer");
      auto &e_viewport_fbuffer_msaa = info.get_resource<gl::Framebuffer>("viewport_draw_begin", "viewport_fbuffer_msaa");

      const auto blit_size = e_viewport_texture.size();
      constexpr auto blit_flags = gl::FramebufferMaskFlags::eColor | gl::FramebufferMaskFlags::eDepth;

      // Blit color results into the single-sampled framebuffer with attached viewport texture
      e_viewport_fbuffer_msaa.blit_to(e_viewport_fbuffer, blit_size, { 0, 0 }, blit_size, { 0, 0 }, blit_flags);
    }
  };
} // namespace met
