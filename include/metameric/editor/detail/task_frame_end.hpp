// Copyright (C) 2024 Mark van de Ruit, Delft University of Technology.
// 
// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.
// 
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
// 
// You should have received a copy of the GNU General Public License
// along with this program.  If not, see <https://www.gnu.org/licenses/>.

#pragma once

#include <metameric/core/scheduler.hpp>
#include <metameric/core/utility.hpp>
#include <metameric/editor/detail/imgui.hpp>
#include <small_gl/framebuffer.hpp>
#include <small_gl/window.hpp>

namespace met::detail {
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
} // namespace met::detail