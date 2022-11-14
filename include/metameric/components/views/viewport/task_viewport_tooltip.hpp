#pragma once

#include <metameric/core/state.hpp>
#include <metameric/core/utility.hpp>
#include <metameric/core/detail/trace.hpp>
#include <metameric/core/detail/scheduler_task.hpp>
#include <metameric/components/views/detail/imgui.hpp>
#include <metameric/components/views/detail/arcball.hpp>
#include <numeric>
#include <ranges>

namespace met {
  class ViewportTooltipTask : public detail::AbstractTask {
  public:
    ViewportTooltipTask(const std::string &name)
    : detail::AbstractTask(name, true) { }

    void init(detail::TaskInitInfo &info) override {
      met_trace_full();
    }
    
    void dstr(detail::TaskDstrInfo &info) override {
      met_trace_full();
    }

    void eval(detail::TaskEvalInfo &info) override {
      met_trace_full();

      constexpr 
      auto i_get = [](auto &v) { return [&v](const auto &i) -> auto& { return v[i]; }; };
    
      // Get shared resources
      auto &io          = ImGui::GetIO();
      auto &i_gamut_ind = info.get_resource<std::vector<uint>>("viewport_input", "gamut_selection");
      auto &i_arcball   = info.get_resource<detail::Arcball>("viewport_input", "arcball");
      auto &e_app_data  = info.get_resource<ApplicationData>(global_key, "app_data");
      auto &e_rgb_gamut = e_app_data.project_data.gamut_colr_i;

      // Compute viewport offset and size, minus ImGui's tab bars etc
      eig::Array2f viewport_offs = static_cast<eig::Array2f>(ImGui::GetWindowPos()) 
                                 + static_cast<eig::Array2f>(ImGui::GetWindowContentRegionMin());
      eig::Array2f viewport_size = static_cast<eig::Array2f>(ImGui::GetWindowContentRegionMax())
                                 - static_cast<eig::Array2f>(ImGui::GetWindowContentRegionMin());
      
      // Gizmo anchor position is mean of selected gamut positions
      auto gamut_selection = i_gamut_ind | std::views::transform(i_get(e_rgb_gamut));
      eig::Vector3f gamut_anchor_pos = std::reduce(range_iter(gamut_selection), Colr(0.f))/ static_cast<float>(gamut_selection.size());
        
      // Compute window-space coordinates
      eig::Array2f p = eig::window_space(gamut_anchor_pos, i_arcball.full(), viewport_offs, viewport_size);
      p.y() += 100.f; // 100 pixels down/up

      ImVec2 m = ImGui::GetIO().MousePos;
      // ImGui::SetNextWindowPos(ImVec2(m.x - 10, m.y));
      ImGui::SetNextWindowPos(p);
      // ImGui::SetNextWindowPos(ImVec2(m.x + 100, m.y));
      ImGui::Begin("1", NULL, ImGuiWindowFlags_Tooltip | ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoTitleBar);
      ImGui::Text("FIRST TOOLTIP");
      ImGui::End();
    }
  };
} // namespace met