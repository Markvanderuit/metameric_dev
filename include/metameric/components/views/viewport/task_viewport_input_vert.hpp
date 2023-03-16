#pragma once

#include <metameric/core/data.hpp>
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
  constexpr float selector_near_distance = 12.f;

  class ViewportInputVertTask : public detail::TaskBase {
    std::vector<Colr> m_colrs_prev;
    bool              m_is_gizmo_used;
    
  public:
    void init(detail::TaskInfo &info) override {
      met_trace_full();

      // Insert shared resources
      info.insert_resource<std::vector<uint>>("selection", { });
      info.insert_resource<std::vector<uint>>("mouseover", { });

      // Start with gizmo inactive
      m_is_gizmo_used = false;
    }

    void eval(detail::TaskInfo &info) override {
      met_trace_full();

      // If active window is not hovered or we are not in vertex mode, exit early
      guard(ImGui::IsItemHovered());

      // Get shared resources
      auto &io          = ImGui::GetIO();
      auto &i_selection = info.get_resource<std::vector<uint>>("selection");
      auto &i_mouseover = info.get_resource<std::vector<uint>>("mouseover");
      auto &e_cstr_slct = info.get_resource<int>("viewport_overlay", "constr_selection");
      auto &i_arcball   = info.get_resource<detail::Arcball>("viewport_input", "arcball");
      auto &e_app_data  = info.get_resource<ApplicationData>(global_key, "app_data");
      auto &e_proj_data = e_app_data.project_data;
      auto &e_verts     = e_app_data.project_data.vertices;

      // Compute viewport offset and size, minus ImGui's tab bars etc
      eig::Array2f viewport_offs = static_cast<eig::Array2f>(ImGui::GetWindowPos()) 
                                 + static_cast<eig::Array2f>(ImGui::GetWindowContentRegionMin());
      eig::Array2f viewport_size = static_cast<eig::Array2f>(ImGui::GetWindowContentRegionMax())
                                 - static_cast<eig::Array2f>(ImGui::GetWindowContentRegionMin());

      // Get vertex colors from vertices
      std::vector<Colr> colrs_i;
      std::ranges::transform(e_verts, std::back_inserter(colrs_i), [](const auto &v) { return v.colr_i; });
      
      // If gizmo is not used or active, handle selection/highlighting
      if ((!ImGuizmo::IsOver() && !ImGuizmo::IsUsing()) || !m_is_gizmo_used) {
        // Declare selector captures to determine point selection/highlighting
        auto selector_ul = eig::Array2f(io.MouseClickedPos[1]).min(eig::Array2f(io.MousePos)).eval();
        auto selector_br = eig::Array2f(io.MouseClickedPos[1]).max(eig::Array2f(io.MousePos)).eval();
        auto selector_rectangle = std::views::filter([&](uint i) {
          eig::Array2f p = eig::world_to_window_space(colrs_i[i], i_arcball.full(), viewport_offs, viewport_size);
          return p.max(selector_ul).min(selector_br).isApprox(p);
        });
        auto selector_near = std::views::filter([&](uint i) {
          eig::Vector2f p = eig::world_to_window_space(colrs_i[i], i_arcball.full(), viewport_offs, viewport_size);
          return (p - eig::Vector2f(io.MousePos)).norm() <= selector_near_distance;
        });

        // Apply mouseover on every iteration
        i_mouseover.clear();
        std::ranges::copy(std::views::iota(0u, e_verts.size()) | selector_near, std::back_inserter(i_mouseover));

        // Apply selection area: right mouse OR left mouse + shift
        if (io.MouseDown[1]) {
          // Add colored rectangles to highlight selection area
          auto col = ImGui::ColorConvertFloat4ToU32(ImGui::GetStyleColorVec4(ImGuiCol_DockingPreview));
          ImGui::GetWindowDrawList()->AddRect(selector_ul, selector_br, col);
          ImGui::GetWindowDrawList()->AddRectFilled(selector_ul, selector_br, col);

          // Push vertex indices on mouseover list
          std::ranges::copy(std::views::iota(0u, e_verts.size()) | selector_rectangle, std::back_inserter(i_mouseover));
        }

        // Right-click-release fixes the selection area; then determine selected gamut position idxs
        if (io.MouseReleased[1]) {
          // Filter tests if a gamut position is inside the selection rectangle in window space
          auto ul = eig::Array2f(io.MouseClickedPos[1]).min(eig::Array2f(io.MousePos)).eval();
          auto br = eig::Array2f(io.MouseClickedPos[1]).max(eig::Array2f(io.MousePos)).eval();
                    
          // Push vertex indices on selection list
          i_selection.clear();
          std::ranges::copy(std::views::iota(0u, e_verts.size()) | selector_rectangle, std::back_inserter(i_selection));
        }

        // Left-click selects a single gamut position
        if (io.MouseClicked[0] && (i_selection.empty() || !ImGuizmo::IsOver())) {
          i_selection.clear();
          std::ranges::copy(std::views::iota(0u, e_verts.size()) | selector_near | std::views::take(1), std::back_inserter(i_selection));
        }
      }

      // Continue only if a selection has been made
      if (i_selection.empty()) {
        m_is_gizmo_used = false;
        return;
      }

      // Sanitize constraint selection index in viewport overlay
      e_cstr_slct = i_selection.empty()
                  ? -1
                  : std::min(e_cstr_slct, 
                             static_cast<int>(e_verts[i_selection[0]].colr_j.size() - 1));

      // Range over- and center of selected gamut positions
      constexpr
      auto i_get = [](auto &v) { return [&v](const auto &i) -> auto& { return v[i]; }; };
      auto selected_verts = i_selection | std::views::transform(i_get(e_verts));
      Colr selected_centr = std::reduce(range_iter(selected_verts), Colr(0.f),
        [](const auto &c, const auto &v) { return c + v.colr_i; }) 
        / static_cast<float>(selected_verts.size()); 

      // ImGuizmo manipulator operates on a transform; to obtain translation
      // distance, we transform a point prior to transformation update
      eig::Affine3f trf_verts = eig::Affine3f(eig::Translation3f(selected_centr));
      eig::Affine3f trf_delta = eig::Affine3f::Identity();

      // Specify ImGuizmo enabled operation; transl for one vertex, transl/rotate for several
      ImGuizmo::OPERATION op = selected_verts.size() > 1 
                             ? ImGuizmo::OPERATION::TRANSLATE | ImGuizmo::OPERATION::ROTATE
                             : ImGuizmo::OPERATION::TRANSLATE;

      // Specify ImGuizmo settings for current viewport and insert the gizmo
      ImGuizmo::SetRect(viewport_offs[0], viewport_offs[1], viewport_size[0], viewport_size[1]);
      ImGuizmo::SetDrawlist(ImGui::GetWindowDrawList());
      ImGuizmo::Manipulate(i_arcball.view().data(), i_arcball.proj().data(), 
        op, ImGuizmo::MODE::LOCAL, trf_verts.data(), trf_delta.data());

      // Register gizmo use start, cache current vertex positions
      if (ImGuizmo::IsUsing() && !m_is_gizmo_used) {
        m_colrs_prev = colrs_i;
        m_is_gizmo_used = true;
      }
      
      // Register gizmo use
      if (ImGuizmo::IsUsing())
        std::ranges::for_each(selected_verts, [&](auto &v) { 
          v.colr_i = (trf_delta * v.colr_i.matrix());
        });

      // Register gizmo use end, update to new vertex positions
      if (!ImGuizmo::IsUsing() && m_is_gizmo_used) {
        // Register data edit as drag finishes
        e_app_data.touch({ 
          .name = "Move gamut points", 
          .redo = [edit = e_verts](auto &data) { data.vertices = edit; }, 
          .undo = [edit = m_colrs_prev](auto &data) { 
            for (uint i = 0; i < edit.size(); ++i)
              data.vertices[i].colr_i = edit[i];
          }
        });
        m_is_gizmo_used = false;
      }
    }
  };
}