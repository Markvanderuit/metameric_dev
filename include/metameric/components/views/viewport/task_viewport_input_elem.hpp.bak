#pragma once

#include <metameric/core/data.hpp>
#include <metameric/core/mesh.hpp>
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
    std::vector<Colr> m_colrs_prev;
    bool              m_is_gizmo_used;

  public:
    ViewportInputElemTask(const std::string &name)
    : detail::AbstractTask(name, true) { }

    void init(detail::TaskInitInfo &info) override {
      met_trace_full();

      // Insert shared resources
      info.insert_resource<std::vector<uint>>("selection", { });
      info.insert_resource<std::vector<uint>>("mouseover", { });

      // Start with gizmo inactive
      m_is_gizmo_used = false;
    }

    void eval(detail::TaskEvalInfo &info) override {
      met_trace_full();

      // If active window is not hovered or we are not in face  mode, exit early
      auto &e_mode = info.get_resource<detail::ViewportInputMode>("viewport_input", "mode");
      guard(e_mode == detail::ViewportInputMode::eFace);
      guard(ImGui::IsItemHovered());

      // Get rest of shared resources
      auto &io               = ImGui::GetIO();
      auto &i_mouseover      = info.get_resource<std::vector<uint>>("mouseover");
      auto &i_selection_elem = info.get_resource<std::vector<uint>>("selection");
      auto &e_selection_vert = info.get_resource<std::vector<uint>>("viewport_input_vert", "selection");
      auto &e_cstr_slct      = info.get_resource<int>("viewport_overlay", "constr_selection");
      auto &i_arcball        = info.get_resource<detail::Arcball>("viewport_input", "arcball");
      auto &e_app_data       = info.get_resource<ApplicationData>(global_key, "app_data");
      auto &e_prj_data       = e_app_data.project_data;
      auto &e_verts          = e_app_data.project_data.gamut_verts;
      auto &e_elems          = e_app_data.project_data.gamut_elems;

      // Compute viewport offset and size, minus ImGui's tab bars etc
      eig::Array2f viewport_offs = static_cast<eig::Array2f>(ImGui::GetWindowPos()) 
                                 + static_cast<eig::Array2f>(ImGui::GetWindowContentRegionMin());
      eig::Array2f viewport_size = static_cast<eig::Array2f>(ImGui::GetWindowContentRegionMax())
                                 - static_cast<eig::Array2f>(ImGui::GetWindowContentRegionMin());

      // Get vertex colors from vertices
      std::vector<Colr> colrs_i;
      std::ranges::transform(e_verts, std::back_inserter(colrs_i), [](const auto &v) { return v.colr_i; });

      // Generate and fire a camera ray against the gamut's triangle elements
      auto screen_pos = eig::window_to_screen_space(io.MousePos, viewport_offs, viewport_size);
      auto camera_ray = i_arcball.generate_ray(screen_pos);
      auto ray_query = raytrace_elem<IndexedMeshData>(camera_ray, { colrs_i, e_elems });

      // Apply mouseover on every iteration
      i_mouseover.clear();
      if (ray_query) {
        i_mouseover.push_back(ray_query.i);
      }

      // Left-click selects an element
      if (io.MouseClicked[0] && (i_selection_elem.empty() || !ImGuizmo::IsOver())) {
        i_selection_elem.clear();
        if (ray_query) {
          i_selection_elem.push_back(ray_query.i);
        }
      }

      // Continue only if a selection has been made
      if (i_selection_elem.empty()) {
        m_is_gizmo_used = false;
        return;
      }

      // Sanitize constraint selection index in viewport overlay
      e_cstr_slct = -1;
      
      // Range over selected elements
      constexpr
      auto i_get = [](auto &v) { return [&v](const auto &i) -> auto& { return v[i]; }; };
      auto selected_elems = i_selection_elem | std::views::transform(i_get(e_elems));

      // Update selected vertex list based on elements
      e_selection_vert.clear();
      for (auto &e : selected_elems) {
        e_selection_vert.push_back(e[0]);
        e_selection_vert.push_back(e[1]);
        e_selection_vert.push_back(e[2]);
      }

      // Range over- and center of selected vertices belonging to triangle
      auto selected_verts = e_selection_vert | std::views::transform(i_get(e_verts));
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
          v.colr_i = (trf_delta * v.colr_i.matrix()).array().min(1.f).max(0.f);
        });

      // Register gizmo use end, update to new vertex positions
      if (!ImGuizmo::IsUsing() && m_is_gizmo_used) {
        // Register data edit as drag finishes
        e_app_data.touch({ 
          .name = "Move gamut points", 
          .redo = [edit = e_verts ](auto &data) { data.gamut_verts = edit; }, 
          .undo = [edit = m_colrs_prev](auto &data) { 
            for (uint i = 0; i < edit.size(); ++i)
              data.gamut_verts[i].colr_i = edit[i];
          }
        });
        m_is_gizmo_used = false;
      }
    }
  };
}