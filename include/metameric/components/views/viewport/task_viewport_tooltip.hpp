#pragma once

#include <metameric/core/state.hpp>
#include <metameric/core/utility.hpp>
#include <metameric/core/detail/trace.hpp>
#include <metameric/core/detail/scheduler_task.hpp>
#include <metameric/components/views/gamut_viewport/task_draw_begin.hpp>
#include <metameric/components/views/gamut_viewport/task_draw_metamer_ocs.hpp>
#include <metameric/components/views/gamut_viewport/task_draw_end.hpp>
#include <metameric/components/views/detail/imgui.hpp>
#include <metameric/components/views/detail/arcball.hpp>
#include <small_gl/texture.hpp>
#include <small_gl/utility.hpp>
#include <ImGuizmo.h>
#include <numeric>
#include <ranges>

namespace met {
  class ViewportTooltipTask : public detail::AbstractTask {
    bool                 m_is_gizmo_used;
    std::array<Colr, 4>  m_offs_prev;
    
  public:
    ViewportTooltipTask(const std::string &name)
    : detail::AbstractTask(name, true) { }

    void init(detail::TaskInitInfo &info) override {
      met_trace_full();

      // Share resources
      info.emplace_resource<gl::Texture2d3f>("draw_texture", { .size = 1 });
      info.emplace_resource<detail::Arcball>("arcball",      { .e_eye = 1.0f, .e_center = 0.0f, .dist_delta_mult = -0.075f });

      // Add subtasks in reverse order
      info.emplace_task_after<DrawEndTask>(name(), name() + "_draw_end", name());
      info.emplace_task_after<DrawMetamerOCSTask>(name(), name() + "_draw_metamer_ocs", name());
      info.emplace_task_after<DrawBeginTask>(name(), name() + "_draw_begin", name());

      // Start with gizmo inactive
      m_is_gizmo_used = false;
    }

    void dstr(detail::TaskDstrInfo &info) {
      met_trace_full();

      // Remove subtasks
      info.remove_task(name() + "_draw_begin");
      info.remove_task(name() + "_draw_metamer_ocs");
      info.remove_task(name() + "_draw_end");
    }

    void eval(detail::TaskEvalInfo &info) override {
      met_trace_full();
    
      // Get shared resources
      auto &e_gamut_index = info.get_resource<std::vector<uint>>("viewport_input", "gamut_selection");

      // Only spawn tooltip on non-empty gamut selection
      guard(!e_gamut_index.empty());

      // Compute viewport offset and size, minus ImGui's tab bars etc
      eig::Array2f viewport_offs = static_cast<eig::Array2f>(ImGui::GetWindowPos()) 
                                 + static_cast<eig::Array2f>(ImGui::GetWindowContentRegionMin());

      auto window_flags = ImGuiWindowFlags_AlwaysAutoResize 
                        | ImGuiWindowFlags_NoDocking 
                        | ImGuiWindowFlags_NoScrollbar
                        | ImGuiWindowFlags_NoResize
                        | ImGuiWindowFlags_NoMove
                        | ImGuiWindowFlags_NoFocusOnAppearing;

      // Spawn window with selection info if one or more vertices are selected
      eig::Array2f ttip_posi = viewport_offs + 16.f, ttip_size = 0.f;
      for (const uint &i : e_gamut_index) {
        ImGui::SetNextWindowPos(ttip_posi);
        if (ImGui::Begin(fmt::format("Vertex {}", i).c_str(), nullptr, window_flags)) {
          eval_single(info, i);
        }
        ttip_size = static_cast<eig::Array2f>(ImGui::GetWindowSize()); // Capture size before to offset next window 
        ttip_posi.y() += ttip_size.y() + 16.f;                         // Offset window position for next window
        ImGui::End();
      }

      // Spawn window for metamer set editing if there is one vertex selected
      if (e_gamut_index.size() == 1) {
        auto imgui_state = { ImGui::ScopedStyleVar(ImGuiStyleVar_WindowPadding, { 0.f, 0.f })};
        ttip_size.y() = 0.f; // Keep same width as previous windows, but have unrestricted height
        ImGui::SetNextWindowPos(ttip_posi);
        ImGui::SetNextWindowSize(ttip_size);
        if (ImGui::Begin("Metamer set", NULL, window_flags | ImGuiWindowFlags_NoTitleBar)) {
          eval_metamer_set(info);
        }
        ImGui::End();
      }
    }

    void eval_single(detail::TaskEvalInfo &info, uint i) {
      met_trace_full();

      // Get shared resources
      auto &e_gamut_spec   = info.get_resource<std::array<Spec, 4>>("gen_spectral_gamut", "gamut_spec");
      auto &e_app_data     = info.get_resource<ApplicationData>(global_key, "app_data");
      auto &e_gamut_colr   = e_app_data.project_data.gamut_colr_i;
      auto &e_gamut_offs   = e_app_data.project_data.gamut_offs_j;
      auto &e_gamut_mapp_i = e_app_data.project_data.gamut_mapp_i;
      auto &e_gamut_mapp_j = e_app_data.project_data.gamut_mapp_j;
      auto &e_mappings     = e_app_data.loaded_mappings;
      auto &e_mapping_data = e_app_data.project_data.mappings;

      // Obtain selected reflectance and colors
      Spec &gamut_spec   = e_gamut_spec[i];
      Colr &gamut_colr_i = e_gamut_colr[i];
      Colr &gamut_offs_j = e_gamut_offs[i];

      // Local copies of gamut mapping indices
      uint l_gamut_mapp_i = e_gamut_mapp_i[i];
      uint l_gamut_mapp_j = e_gamut_mapp_j[i];

      // Compute resulting color and error
      Colr gamut_actual = e_mappings[e_gamut_mapp_i[i]].apply_color(gamut_spec);
      Colr gamut_error  = (gamut_actual - gamut_colr_i).abs();

      // Plot of solved-for reflectance
      ImGui::PlotLines("Reflectance", gamut_spec.data(), gamut_spec.rows(), 0, nullptr, 0.f, 1.f, { 0.f, 64.f });

      ImGui::Separator();

      ImGui::Text("Color input");
      ImGui::ColorEdit3("Value",  gamut_colr_i.data(), ImGuiColorEditFlags_Float);
      ImGui::ColorEdit3("Offset", gamut_offs_j.data(), ImGuiColorEditFlags_Float);
  
      ImGui::Separator();

      ImGui::Text("Color output");
      ImGui::ColorEdit3("Value", gamut_actual.data(), ImGuiColorEditFlags_Float);
      ImGui::ColorEdit3("Error", gamut_error.data(), ImGuiColorEditFlags_Float);

      ImGui::Separator();

      // Selector for first mapping index
      if (ImGui::BeginCombo("Mapping 0", e_mapping_data[l_gamut_mapp_i].first.c_str())) {
        for (uint j = 0; j < e_mapping_data.size(); ++j) {
          auto &[key, _] = e_mapping_data[j];
          if (ImGui::Selectable(key.c_str(), j == l_gamut_mapp_i)) {
            l_gamut_mapp_i = j;
          }
        }
        ImGui::EndCombo();
      }

      // Selector for second mapping index
      if (ImGui::BeginCombo("Mapping 1", e_mapping_data[l_gamut_mapp_j].first.c_str())) {
        for (uint j = 0; j < e_mapping_data.size(); ++j) {
          auto &[key, _] = e_mapping_data[j];
          if (ImGui::Selectable(key.c_str(), j == l_gamut_mapp_j)) {
            l_gamut_mapp_j = j;
          }
        }
        ImGui::EndCombo();
      }

      // If changes to local copies were made, register a data edit
      if (l_gamut_mapp_i != e_gamut_mapp_i[i]) {
        e_app_data.touch({
          .name = "Change gamut mapping 0",
          .redo = [edit = l_gamut_mapp_i,                j = i](auto &data) { data.gamut_mapp_i[j] = edit; },
          .undo = [edit = e_gamut_mapp_i[i], j = i](auto &data) { data.gamut_mapp_i[j] = edit; }
        });
      }
      if (l_gamut_mapp_j != e_gamut_mapp_j[i]) {
        e_app_data.touch({
          .name = "Change gamut mapping 1",
          .redo = [edit = l_gamut_mapp_j,                j = i](auto &data) { data.gamut_mapp_j[j] = edit; },
          .undo = [edit = e_gamut_mapp_j[i], j = i](auto &data) { data.gamut_mapp_j[j] = edit; }
        });
      }
    }

    void eval_metamer_set(detail::TaskEvalInfo &info) {
      met_trace_full();
        
      // Get shared resources
      auto &i_draw_texture = info.get_resource<gl::Texture2d3f>("draw_texture");
      auto &e_gamut_index  = info.get_resource<std::vector<uint>>("viewport_input", "gamut_selection")[0];

      // Compute viewport size minus ImGui's tab bars etc
      // (Re-)create viewport texture if necessary; attached framebuffers are resized separately
      eig::Array2f viewport_size = static_cast<eig::Array2f>(ImGui::GetWindowContentRegionMax())
                                 - static_cast<eig::Array2f>(ImGui::GetWindowContentRegionMin());
      eig::Array2f texture_size  = viewport_size.x();
      if (!i_draw_texture.is_init() || (i_draw_texture.size() != viewport_size.cast<uint>()).all()) {
        i_draw_texture = {{ .size = texture_size.cast<uint>() }};
      }

      // Insert image, applying viewport texture to viewport; texture can be safely drawn 
      // to later in the render loop. Flip y-axis UVs to obtain the correct orientation.
      ImGui::Image(ImGui::to_ptr(i_draw_texture.object()), texture_size, eig::Vector2f(0, 1), eig::Vector2f(1, 0));
      
      // Handle input
      if (ImGui::IsItemHovered()) {
        if (!ImGuizmo::IsUsing()) {
          eval_camera(info);
        }
        eval_gizmo(info);
      }
    }

    void eval_camera(detail::TaskEvalInfo &info) {
      met_trace_full();
      
      // Get shared resources
      auto &io        = ImGui::GetIO();
      auto &i_arcball = info.get_resource<detail::Arcball>("arcball");
      auto &i_texture = info.get_resource<gl::Texture2d3f>("draw_texture");

      // Update camera info: aspect ratio, scroll delta, move delta
      i_arcball.m_aspect = i_texture.size().x() / i_texture.size().y();
      i_arcball.set_dist_delta(io.MouseWheel);
      if (io.MouseDown[2] || (io.MouseDown[0] && io.KeyCtrl)) {
        i_arcball.set_pos_delta(eig::Array2f(io.MouseDelta) / i_texture.size().cast<float>());
      }
      i_arcball.update_matrices();
    }

    void eval_gizmo(detail::TaskEvalInfo &info) {
      met_trace_full();

      // Get shared resources
      auto &i_arcball    = info.get_resource<detail::Arcball>("arcball");
      auto &e_gamut_idx  = info.get_resource<std::vector<uint>>("viewport_input", "gamut_selection")[0];
      auto &e_ocs_centr  = info.get_resource<Colr>("gen_metamer_ocs", fmt::format("ocs_center_{}", e_gamut_idx));
      auto &e_app_data   = info.get_resource<ApplicationData>(global_key, "app_data");
      auto &e_gamut_colr = e_app_data.project_data.gamut_colr_i;
      auto &e_gamut_offs = e_app_data.project_data.gamut_offs_j;

      // Anchor position is colr + offset, minus center offset 
      eig::Array3f anchor_pos = e_gamut_colr[e_gamut_idx] + e_gamut_offs[e_gamut_idx] - e_ocs_centr;
      auto anchor_trf = eig::Affine3f(eig::Translation3f(anchor_pos));
      auto pre_pos    = anchor_trf * eig::Vector3f(0, 0, 0);

      // Insert ImGuizmo manipulator at anchor position
      eig::Vector2f rmin = ImGui::GetItemRectMin(), rmax = ImGui::GetItemRectSize();
      ImGuizmo::SetRect(rmin.x(), rmin.y(), rmax.x(), rmax.y());
      ImGuizmo::SetDrawlist(ImGui::GetWindowDrawList());
      ImGuizmo::Manipulate(i_arcball.view().data(), 
                           i_arcball.proj().data(),
                           ImGuizmo::OPERATION::TRANSLATE, 
                           ImGuizmo::MODE::LOCAL, 
                           anchor_trf.data());

      // After transformation update, we transform a second point to obtain translation distance
      auto post_pos = anchor_trf * eig::Vector3f(0, 0, 0);
      auto transl   = (post_pos - pre_pos).eval();

      // Start gizmo drag
      if (ImGuizmo::IsUsing() && !m_is_gizmo_used) {
        m_is_gizmo_used = true;
        m_offs_prev = e_gamut_offs;
      }

      // Halfway gizmo drag
      if (ImGuizmo::IsUsing()) {
        e_gamut_offs[e_gamut_idx] = (e_gamut_offs[e_gamut_idx] + transl.array()).eval();
      }

      // End gizmo drag
      if (!ImGuizmo::IsUsing() && m_is_gizmo_used) {
        m_is_gizmo_used = false;
        
        // Register data edit as drag finishes
        e_app_data.touch({ 
          .name = "Move gamut offsets", 
          .redo = [edit = e_gamut_offs](auto &data) { data.gamut_offs_j = edit; }, 
          .undo = [edit = m_offs_prev](auto &data) { data.gamut_offs_j = edit; }
        });
      }
    }
  };
} // namespace met