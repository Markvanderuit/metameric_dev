#pragma once

#include <metameric/core/utility.hpp>
#include <metameric/core/detail/trace.hpp>
#include <metameric/core/detail/scheduler_task.hpp>
#include <small_gl/framebuffer.hpp>
#include <small_gl/texture.hpp>

namespace met {

  class DrawEndTask : public detail::AbstractTask {
    std::string m_parent;

  public:
    DrawEndTask(const std::string &name, const std::string &parent)
    : detail::AbstractTask(name, true),
      m_parent(parent) { }

    void eval(detail::TaskEvalInfo &info) override {
      met_trace_full();
      constexpr auto task_begin_fmt = FMT_COMPILE("{}_draw_begin");
    
      // Get shared resources 
      auto &e_target_texture  = info.get_resource<gl::Texture2d3f>(m_parent, "draw_texture");
      auto &e_frame_buffer    = info.get_resource<gl::Framebuffer>(fmt::format(task_begin_fmt, m_parent), "frame_buffer");
      auto &e_frame_buffer_ms = info.get_resource<gl::Framebuffer>(fmt::format(task_begin_fmt, m_parent), "frame_buffer_ms");

      const auto blit_size = e_target_texture.size();
      constexpr auto blit_flags = gl::FramebufferMaskFlags::eColor | gl::FramebufferMaskFlags::eDepth;

      // Blit color results into the single-sampled framebuffer with attached viewport texture
      e_frame_buffer_ms.blit_to(e_frame_buffer, blit_size, 0u, blit_size, 0u, blit_flags);
    }
  };
} // namespace  met
