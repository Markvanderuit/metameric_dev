#pragma once

#include <metameric/core/data.hpp>
#include <metameric/core/scheduler.hpp>
#include <metameric/core/utility.hpp>
#include <metameric/components/views/detail/arcball.hpp>
#include <metameric/components/views/detail/imgui.hpp>
#include <small_gl/window.hpp>

namespace met {
  struct MeshViewportInputTask : public detail::TaskNode {
    void init(SchedulerHandle &info) override {
      met_trace();
      
      info.resource("arcball").init<detail::Arcball>({ 
        .dist            = 1.f,
        .e_eye           = 1.f,
        .e_center        = 0.f,
        .zoom_delta_mult = 0.1f
      });
    }

    void eval(SchedulerHandle &info) override {
      met_trace();

      // If window is not hovered, exit now instead of handling camera input
      guard(ImGui::IsItemHovered());

      // Get modified resources
      auto &io = ImGui::GetIO();
      auto &i_arcball = info.resource("arcball").writeable<detail::Arcball>();

      // Compute viewport offs, size minus ImGui's tab bars etc
      eig::Array2f viewport_offs = static_cast<eig::Array2f>(ImGui::GetWindowPos()) 
                                 + static_cast<eig::Array2f>(ImGui::GetWindowContentRegionMin());
      eig::Array2f viewport_size = static_cast<eig::Array2f>(ImGui::GetWindowContentRegionMax())
                                 - static_cast<eig::Array2f>(ImGui::GetWindowContentRegionMin());    

      // Handle mouse panning data
      i_arcball.m_aspect = viewport_size.x() / viewport_size.y();
      if (io.MouseWheel != 0.f)
        i_arcball.set_zoom_delta(-io.MouseWheel);
      if (io.MouseDown[0])
        i_arcball.set_ball_delta(eig::Array2f(io.MouseDelta) / viewport_size.array());
      if (io.MouseDown[1]) {
        i_arcball.set_move_delta((eig::Array3f() 
          << eig::Array2f(io.MouseDelta.x, io.MouseDelta.y) / viewport_size.array(), 0).finished());
      }
    }
  };
} // namespace met