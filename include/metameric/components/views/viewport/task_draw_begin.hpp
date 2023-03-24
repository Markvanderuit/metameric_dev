#pragma once

#include <metameric/core/data.hpp>
#include <metameric/core/scheduler.hpp>
#include <metameric/core/utility.hpp>
#include <small_gl/framebuffer.hpp>
#include <small_gl/renderbuffer.hpp>
#include <small_gl/texture.hpp>

namespace met {
  class ViewportDrawBeginTask : public detail::TaskNode {
    using Colorbuffer = gl::Renderbuffer<float, 4, gl::RenderbufferType::eMultisample>;
    using Depthbuffer = gl::Renderbuffer<gl::DepthComponent, 1, gl::RenderbufferType::eMultisample>;

    // Framebuffer attachments
    Colorbuffer m_color_buffer_ms;
    Depthbuffer m_depth_buffer_ms;
    
  public:
    void init(SchedulerHandle &info) override {
      met_trace_full();
    
      // Share uninitialized framebuffer objects; initialized during eval()
      info.resource("frame_buffer").set<gl::Framebuffer>({ });
      info.resource("frame_buffer_msaa").set<gl::Framebuffer>({ });
    }

    void eval(SchedulerHandle &info) override {
      met_trace_full();
    
      // Get external resources 
      const auto &e_appl_data   = info.global("app_data").read_only<ApplicationData>();
      const auto &e_lrgb_target = info.resource("viewport.begin", "lrgb_target").read_only<gl::Texture2d4f>();

      // Get modified resources 
      auto &i_frame_buffer    = info.resource("frame_buffer").writeable<gl::Framebuffer>();
      auto &i_frame_buffer_ms = info.resource("frame_buffer_msaa").writeable<gl::Framebuffer>();

      // (Re-)create framebuffers and renderbuffers if the viewport has resized
      if (!i_frame_buffer.is_init() || (e_lrgb_target.size() != m_color_buffer_ms.size()).any()) {
        m_color_buffer_ms = {{ .size = e_lrgb_target.size().max(1) }};
        m_depth_buffer_ms = {{ .size = e_lrgb_target.size().max(1) }};
        i_frame_buffer_ms = {{ .type = gl::FramebufferType::eColor, .attachment = &m_color_buffer_ms },
                             { .type = gl::FramebufferType::eDepth, .attachment = &m_depth_buffer_ms }};
        i_frame_buffer    = {{ .type = gl::FramebufferType::eColor, .attachment = &e_lrgb_target }};
      }

      eig::Array4f clear_colr = e_appl_data.color_mode == AppColorMode::eDark
                              ? eig::Array4f { 0, 0, 0, 1 } 
                              : ImGui::GetStyleColorVec4(ImGuiCol_ChildBg);

      // Clear framebuffer target for next subtasks
      i_frame_buffer_ms.clear(gl::FramebufferType::eColor, clear_colr);
      i_frame_buffer_ms.clear(gl::FramebufferType::eDepth, 1.f);
      i_frame_buffer_ms.bind();

      // Specify viewport for next subtasks
      gl::state::set_viewport(m_color_buffer_ms.size());    

      // Specify depth state for next tasks
      gl::state::set_depth_range(0.f, 1.f);
      gl::state::set_op(gl::DepthOp::eLess);
    }
  };
} // namespace met
