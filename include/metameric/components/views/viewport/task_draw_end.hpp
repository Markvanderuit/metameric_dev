#pragma once

#include <metameric/core/utility.hpp>
#include <metameric/core/detail/scheduler_task.hpp>
#include <metameric/core/detail/trace.hpp>
#include <small_gl/framebuffer.hpp>
#include <small_gl/texture.hpp>

namespace met {
  struct ViewportDrawEndTask : public detail::AbstractTask {
    ViewportDrawEndTask(const std::string &name)
    : detail::AbstractTask(name, true) { }

    void eval(detail::TaskEvalInfo &info) override {
      met_trace_full();
    
      // Get shared resources 
      auto &e_draw_texture    = info.get_resource<gl::Texture2d3f>("viewport_begin", "draw_texture");
      auto &e_frame_buffer    = info.get_resource<gl::Framebuffer>("viewport_draw_begin", "frame_buffer");
      auto &e_frame_buffer_ms = info.get_resource<gl::Framebuffer>("viewport_draw_begin", "frame_buffer_msaa");

      const auto blit_size = e_draw_texture.size();
      constexpr auto blit_flags = gl::FramebufferMaskFlags::eColor | gl::FramebufferMaskFlags::eDepth;

      // Blit color results into the single-sampled framebuffer with attached viewport texture
      e_frame_buffer_ms.blit_to(e_frame_buffer, blit_size, 0u, blit_size, 0u, blit_flags);
    }
  };
} // namespace met
