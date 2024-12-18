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

#include <metameric/scene/scene.hpp>
#include <metameric/core/scheduler.hpp>
#include <metameric/editor/detail/arcball.hpp>
#include <metameric/editor/detail/imgui.hpp>
#include <small_gl/texture.hpp>
#include <small_gl/window.hpp>

namespace met::detail {
  struct ArcballInputTask : public detail::TaskNode {
    using InfoType = detail::ArcballInfo;
    
  private:
    InfoType       m_info;
    ResourceHandle m_view_handle;

  public:
    // Constructor defaults to sensible arcball settings
    // - prior; arcb_handle to corresponding target viewport; should hold gl::Texture2d4f
    // - info; arcball initialization settings
    ArcballInputTask(ResourceHandle view, InfoType info = {
      .dist            = 3.4641f,
      .e_eye           = 1.f,
      .e_center        = 0.f,
      .zoom_delta_mult = 0.1f
    }) : m_view_handle(view), m_info(info) { }

    void init(SchedulerHandle &info) override {
      met_trace();
      info("arcball").init<Arcball>(m_info);
    }

    void eval(SchedulerHandle &info) override {
      met_trace();
      
      // Handle to the view resource is masked, s.t. we can query it directly
      m_view_handle.reinitialize(info);
      
      // Get relevant handles and resources
      auto arcb_handle   = info("arcball");
      const auto &io     = ImGui::GetIO();
      const auto &e_view = m_view_handle.getr<gl::Texture2d4f>();

      // Get float representation of view size
      auto view_size = e_view.size().cast<float>().eval();
      
      // On viewport change, arcb_handle aspect ratio      
      if (m_view_handle.is_mutated() || is_first_eval()) {
        arcb_handle.getw<detail::Arcball>()
                   .set_aspect(view_size.x() / view_size.y());
      }
      
      // If enclosing viewport is not hovered, 
      // exit now instead of handling user input
      guard(ImGui::IsItemHovered());

      // Handle mouse scroll
      if (io.MouseWheel != 0.f) {
        arcb_handle.getw<detail::Arcball>()
                   .set_zoom_delta(-io.MouseWheel);
      }

      // Handle right mouse controll
      if (io.MouseDown[1]) {
        arcb_handle.getw<detail::Arcball>()
                   .set_ball_delta(eig::Array2f(io.MouseDelta) / view_size.array());
      }

      // Handle middle mouse controll
      if (io.MouseDown[2]) {
        auto move_delta = (eig::Array3f() 
          << eig::Array2f(io.MouseDelta.x, io.MouseDelta.y) / view_size.array(), 0).finished();
        arcb_handle.getw<detail::Arcball>()
              .set_move_delta(move_delta);
      }
    }
  };
} // namespace met::detail