#pragma once

#include <metameric/core/data.hpp>
#include <metameric/core/scheduler.hpp>
#include <metameric/core/utility.hpp>
#include <metameric/components/views/detail/panscan.hpp>
#include <metameric/components/views/detail/imgui.hpp>
#include <small_gl/window.hpp>

namespace met {
  struct EmbeddingViewportInputTask : public detail::TaskNode {
    void init(SchedulerHandle &info) override {
      met_trace();
      info.resource("panscan").init<detail::Panscan>({ 
        .scale            = 0.01f,
        .pos_delta_mult   = 2.f,
        .scale_delta_mult = 0.0001f,
        .scale_delta_curv = 4.f
      });
    }

    void eval(SchedulerHandle &info) override {
      met_trace();

      // If window is not hovered, exit now instead of handling camera input
      guard(ImGui::IsItemHovered());

      // Get modified resources
      auto &io = ImGui::GetIO();
      auto &i_panscan = info.resource("panscan").getw<detail::Panscan>();

      // Compute viewport offs, size minus ImGui's tab bars etc
      eig::Array2f viewport_offs = static_cast<eig::Array2f>(ImGui::GetWindowPos()) 
                                 + static_cast<eig::Array2f>(ImGui::GetWindowContentRegionMin());
      eig::Array2f viewport_size = static_cast<eig::Array2f>(ImGui::GetWindowContentRegionMax())
                                 - static_cast<eig::Array2f>(ImGui::GetWindowContentRegionMin());    

      // Handle mouse panning data
      i_panscan.m_aspect = viewport_size;
      if (io.MouseWheel != 0.f)
        i_panscan.set_scale_delta(-io.MouseWheel);
      if (io.MouseDown[0])
        i_panscan.set_pos_delta(eig::Array2f(io.MouseDelta));
      i_panscan.update_matrices();
    }
  };
} // namespace met