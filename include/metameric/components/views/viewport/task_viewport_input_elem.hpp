#pragma once

#include <metameric/core/state.hpp>
#include <metameric/core/utility.hpp>
#include <metameric/core/detail/trace.hpp>
#include <metameric/core/detail/scheduler_task.hpp>
#include <metameric/components/views/detail/arcball.hpp>
#include <metameric/components/views/detail/enum.hpp>
#include <metameric/components/views/detail/imgui.hpp>
#include <ImGuizmo.h>
#include <algorithm>
#include <numeric>
#include <ranges>

namespace met {
  class ViewportInputElemTask : public detail::AbstractTask {
    std::vector<Colr> m_verts_prev;
    bool              m_is_gizmo_used;

  public:
    ViewportInputElemTask(const std::string &name)
    : detail::AbstractTask(name, true) { }

    void init(detail::TaskInitInfo &info) override {
      met_trace_full();

      // Insert shared resources
      info.insert_resource<std::vector<uint>>("selection", { });

      // Start with gizmo inactive
      m_is_gizmo_used = false;
    }

    void eval(detail::TaskEvalInfo &info) override {
      met_trace_full();

      // If active window is not hovered or we are not in face  mode, exit early
      auto &e_mode      = info.get_resource<detail::ViewportInputMode>("viewport_input", "mode");
      guard(e_mode == detail::ViewportInputMode::eFace);
      guard(ImGui::IsItemHovered());

      // Get rest of shared resources
      auto &io           = ImGui::GetIO();
      auto &i_selection = info.get_resource<std::vector<uint>>("selection");
      auto &i_arcball    = info.get_resource<detail::Arcball>("viewport_input", "arcball");
      auto &e_app_data   = info.get_resource<ApplicationData>(global_key, "app_data");
      auto &e_proj_data  = e_app_data.project_data;
      auto &e_verts      = e_app_data.project_data.gamut_colr_i;
      auto &e_elems      = e_app_data.project_data.gamut_elems;

      // Compute viewport offset and size, minus ImGui's tab bars etc
      eig::Array2f viewport_offs = static_cast<eig::Array2f>(ImGui::GetWindowPos()) 
                                 + static_cast<eig::Array2f>(ImGui::GetWindowContentRegionMin());
      eig::Array2f viewport_size = static_cast<eig::Array2f>(ImGui::GetWindowContentRegionMax())
                                 - static_cast<eig::Array2f>(ImGui::GetWindowContentRegionMin());

      // ...
    }
  };
}