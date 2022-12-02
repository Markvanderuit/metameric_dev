#pragma once

#include <metameric/core/spectrum.hpp>
#include <metameric/core/state.hpp>
#include <metameric/core/utility.hpp>
#include <metameric/core/detail/trace.hpp>
#include <metameric/core/detail/scheduler_task.hpp>
#include <metameric/components/views/detail/arcball.hpp>
#include <metameric/components/views/detail/imgui.hpp>
#include <metameric/components/views/detail/enum.hpp>
#include <metameric/components/views/viewport/task_viewport_input_vert.hpp>
#include <metameric/components/views/viewport/task_viewport_input_edge.hpp>
#include <metameric/components/views/viewport/task_viewport_input_elem.hpp>
#include <ImGuizmo.h>
#include <algorithm>
#include <functional>
#include <numeric>
#include <ranges>

namespace met {

  class ViewportInputTask : public detail::AbstractTask {
  public:
    ViewportInputTask(const std::string &name)
    : detail::AbstractTask(name, true) { }

    void init(detail::TaskInitInfo &info) override {
      met_trace_full();
    
      // Add subtasks
      info.emplace_task_after<ViewportInputVertTask>(name(), name() + "_vert");
      info.emplace_task_after<ViewportInputEdgeTask>(name(), name() + "_edge");
      info.emplace_task_after<ViewportInputElemTask>(name(), name() + "_elem");

      // Share resources
      info.emplace_resource<detail::ViewportInputMode>("mode", detail::ViewportInputMode::eVertex);
      info.emplace_resource<detail::Arcball>("arcball", { .e_eye = 1.5f, .e_center = 0.5f });
    }

    void dstr(detail::TaskDstrInfo &info) override {
      met_trace_full();

      // Remove subtasks
      info.remove_task(name() + "_vert");
      info.remove_task(name() + "_edge");
      info.remove_task(name() + "_elem");
    }

    void eval(detail::TaskEvalInfo &info) override {
      met_trace_full();
                      
      // Get shared resources
      auto &io        = ImGui::GetIO();
      auto &i_arcball = info.get_resource<detail::Arcball>("arcball");
      auto &i_mode    = info.get_resource<detail::ViewportInputMode>("mode");

      // Compute viewport offs, size minus ImGui's tab bars etc
      eig::Array2f viewport_offs = static_cast<eig::Array2f>(ImGui::GetWindowPos()) 
                                 + static_cast<eig::Array2f>(ImGui::GetWindowContentRegionMin());
      eig::Array2f viewport_size = static_cast<eig::Array2f>(ImGui::GetWindowContentRegionMax())
                                 - static_cast<eig::Array2f>(ImGui::GetWindowContentRegionMin());     

      // Handle edit mode selection window
      constexpr auto window_flags = 
        ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoDocking | ImGuiWindowFlags_NoScrollbar | 
        ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove  | ImGuiWindowFlags_NoFocusOnAppearing;
      eig::Array2f edit_size = { 300.f, 100.f };
      eig::Array2f edit_posi = { viewport_offs.x() + viewport_size.x() - edit_size.x() - 16.f, viewport_offs.y() + 16.f };
      ImGui::SetNextWindowPos(edit_posi);
      ImGui::SetNextWindowSize(edit_size);
      if (ImGui::Begin("Edit mode", nullptr, window_flags)) {
        int m = static_cast<int>(i_mode);
        ImGui::RadioButton("Vertex", &m, static_cast<int>(detail::ViewportInputMode::eVertex));
        ImGui::SameLine();
        ImGui::RadioButton("Edge",   &m, static_cast<int>(detail::ViewportInputMode::eEdge));
        ImGui::SameLine();
        ImGui::RadioButton("Face",   &m, static_cast<int>(detail::ViewportInputMode::eFace));
        if (auto mode = detail::ViewportInputMode(m); mode != i_mode) {
          // Reset selections
          info.get_resource<std::vector<uint>>("viewport_input_vert", "selection").clear();
          info.get_resource<std::vector<uint>>("viewport_input_elem", "selection").clear();
          i_mode = mode;
        }
      }
      ImGui::End();

      // If window is not hovered, exit now instead of handling camera input
      guard(ImGui::IsItemHovered());

      // Handle camera update: aspect ratio, scroll delta, move delta dependent on ImGui i/o
      i_arcball.m_aspect = viewport_size.x() / viewport_size.y();
      i_arcball.set_dist_delta(-0.5f * io.MouseWheel);
      if (io.MouseDown[2] || (io.MouseDown[0] && io.KeyCtrl))
        i_arcball.set_pos_delta(eig::Array2f(io.MouseDelta) / viewport_size.array());
      i_arcball.update_matrices();
    }
  };
} // namespace met