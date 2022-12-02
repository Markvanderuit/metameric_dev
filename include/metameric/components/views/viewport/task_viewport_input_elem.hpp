#pragma once

#include <metameric/core/state.hpp>
#include <metameric/core/ray.hpp>
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
      auto &io               = ImGui::GetIO();
      auto &i_selection_elem = info.get_resource<std::vector<uint>>("selection");
      auto &i_selection_vert = info.get_resource<std::vector<uint>>("viewport_input_vert", "selection");
      auto &i_arcball        = info.get_resource<detail::Arcball>("viewport_input", "arcball");
      auto &e_app_data       = info.get_resource<ApplicationData>(global_key, "app_data");
      auto &e_proj_data      = e_app_data.project_data;
      auto &e_verts          = e_app_data.project_data.gamut_colr_i;
      auto &e_elems          = e_app_data.project_data.gamut_elems;

      // Compute viewport offset and size, minus ImGui's tab bars etc
      eig::Array2f viewport_offs = static_cast<eig::Array2f>(ImGui::GetWindowPos()) 
                                 + static_cast<eig::Array2f>(ImGui::GetWindowContentRegionMin());
      eig::Array2f viewport_size = static_cast<eig::Array2f>(ImGui::GetWindowContentRegionMax())
                                 - static_cast<eig::Array2f>(ImGui::GetWindowContentRegionMin());

      // Left-click selects a vertex
      if (io.MouseClicked[0] && (i_selection_elem.empty() || !ImGuizmo::IsOver())) {
        // Generate a camera ray
        auto screen_pos = eig::window_to_screen_space(io.MouseClickedPos[0], viewport_offs, viewport_size);
        auto camera_ray = i_arcball.generate_ray(screen_pos);
        
        // Perform ray-tracing operation to select nearest triangle
        i_selection_elem.clear();
        if (auto query = ray_trace_nearest_elem(camera_ray, e_verts, e_elems); query) {
          i_selection_elem.push_back(query.i);
        }
      }

      // Continue only if a selection has been made
      guard(!i_selection_elem.empty());

      // Range over- and center of selected vertices belonging to tirangle
      constexpr
      auto i_get = [](auto &v) { return [&v](const auto &i) -> auto& { return v[i]; }; };
      auto selected_elems = i_selection_elem | std::views::transform(i_get(e_elems));
      i_selection_vert.clear();
      for (auto &e : selected_elems) {
        i_selection_vert.push_back(e[0]);
        i_selection_vert.push_back(e[1]);
        i_selection_vert.push_back(e[2]);
      }
      auto selected_verts = i_selection_vert | std::views::transform(i_get(e_verts));
      Colr selected_centr = std::reduce(range_iter(selected_verts), Colr(0.f)) 
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
        m_verts_prev = e_verts;
        m_is_gizmo_used = true;
      }
      
      // Register gizmo use
      if (ImGuizmo::IsUsing())
        std::ranges::for_each(selected_verts, [&](Colr &p) { 
          p = (trf_delta * p.matrix()).array().min(1.f).max(0.f);
        });

      // Register gizmo use end, update to new vertex positions
      if (!ImGuizmo::IsUsing() && m_is_gizmo_used) {
        // Register data edit as drag finishes
        e_app_data.touch({ 
          .name = "Move gamut points", 
          .redo = [edit = e_verts ](auto &data) { data.gamut_colr_i = edit; }, 
          .undo = [edit = m_verts_prev](auto &data) { data.gamut_colr_i = edit; }
        });
        m_is_gizmo_used = false;
      }
    }
  };
}