#pragma once

#include <metameric/core/state.hpp>
#include <metameric/core/utility.hpp>
#include <metameric/core/detail/trace.hpp>
#include <metameric/core/detail/scheduler_task.hpp>
#include <metameric/components/views/viewport/task_draw_color_solid.hpp>
#include <metameric/components/views/detail/imgui.hpp>
#include <metameric/components/views/detail/arcball.hpp>
#include <small_gl/texture.hpp>
#include <small_gl/utility.hpp>
#include <ImGuizmo.h>
#include <numeric>
#include <ranges>

namespace met {
  class ViewportTooltipTask : public detail::AbstractTask {
    std::vector<Colr> m_offs_prev;
    bool              m_is_gizmo_used;
    
  public:
    ViewportTooltipTask(const std::string &name)
    : detail::AbstractTask(name, true) { }

    void init(detail::TaskInitInfo &info) override {
      met_trace_full();

      // Share resources
      info.emplace_resource<gl::Texture2d4f>("draw_texture",      { .size = 1 });
      info.emplace_resource<gl::Texture2d4f>("draw_texture_srgb", { .size = 1 });
      info.emplace_resource<detail::Arcball>("arcball",           { .e_eye = 1.0f, .e_center = 0.0f, .dist_delta_mult = -0.075f });

      // Add subtasks
      info.emplace_task_after<DrawColorSolidTask>(name(), name() + "_draw_color_solid", name());

      // Start with gizmo inactive
      m_is_gizmo_used = false;
    }

    void dstr(detail::TaskDstrInfo &info) {
      met_trace_full();

      // Remove subtasks
      info.remove_task(name() + "_draw_color_solid");
    }

    void eval(detail::TaskEvalInfo &info) override {
      met_trace_full();
    
      // Get shared resources
      auto &e_gamut_index = info.get_resource<std::vector<uint>>("viewport_input_vert", "selection");

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
      auto &e_gamut_spec   = info.get_resource<std::vector<Spec>>("gen_spectral_gamut", "gamut_spec");
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
      Colr gamut_colr_j = gamut_colr_i + gamut_offs_j;
      Colr gamut_actual_i = e_mappings[e_gamut_mapp_i[i]].apply_color(gamut_spec);
      Colr gamut_actual_j = e_mappings[e_gamut_mapp_j[i]].apply_color(gamut_spec);
      Colr gamut_error_i  = (gamut_actual_i - gamut_colr_i).abs();
      Colr gamut_error_j  = (gamut_actual_j - gamut_colr_j).abs();

      // Get gamma-corrected colors
      Colr gamut_colr_i_srgb = linear_srgb_to_gamma_srgb(gamut_colr_i);
      Colr gamut_colr_j_srgb = linear_srgb_to_gamma_srgb(gamut_colr_j);
      Colr gamut_actual_i_srgb = linear_srgb_to_gamma_srgb(gamut_actual_i);
      Colr gamut_actual_j_srgb = linear_srgb_to_gamma_srgb(gamut_actual_j);

      // Plot of solved-for reflectance
      ImGui::PlotLines("Reflectance", gamut_spec.data(), gamut_spec.rows(), 0, nullptr, 0.f, 1.f, { 0.f, 64.f });

      ImGui::Separator();

      ImGui::Text("Color values");
      ImGui::ColorEdit3("Value 0", gamut_colr_i_srgb.data(), ImGuiColorEditFlags_Float);
      ImGui::ColorEdit3("Value 1", gamut_colr_j_srgb.data(), ImGuiColorEditFlags_Float);
  
      ImGui::Separator();

      ImGui::Text("Color roundtrip");
      ImGui::ColorEdit3("Value, 0", gamut_actual_i_srgb.data(), ImGuiColorEditFlags_Float | ImGuiColorEditFlags_NoInputs);
      ImGui::SameLine();
      ImGui::ColorEdit3("Value, 1", gamut_actual_j_srgb.data(), ImGuiColorEditFlags_Float | ImGuiColorEditFlags_NoInputs);
      ImGui::ColorEdit3("Error, 0", gamut_error_i.data(), ImGuiColorEditFlags_Float | ImGuiColorEditFlags_NoInputs);
      ImGui::SameLine();
      ImGui::ColorEdit3("Error, 1", gamut_error_j.data(), ImGuiColorEditFlags_Float | ImGuiColorEditFlags_NoInputs);

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
          .redo = [edit = l_gamut_mapp_i,    j = i](auto &data) { data.gamut_mapp_i[j] = edit; },
          .undo = [edit = e_gamut_mapp_i[i], j = i](auto &data) { data.gamut_mapp_i[j] = edit; }
        });
      }
      if (l_gamut_mapp_j != e_gamut_mapp_j[i]) {
        e_app_data.touch({
          .name = "Change gamut mapping 1",
          .redo = [edit = l_gamut_mapp_j,    j = i](auto &data) { data.gamut_mapp_j[j] = edit; },
          .undo = [edit = e_gamut_mapp_j[i], j = i](auto &data) { data.gamut_mapp_j[j] = edit; }
        });
      }
    }

    void eval_metamer_set(detail::TaskEvalInfo &info) {
      met_trace_full();
        
      // Get shared resources
      auto &i_draw_texture      = info.get_resource<gl::Texture2d4f>("draw_texture");
      auto &i_draw_texture_srgb = info.get_resource<gl::Texture2d4f>("draw_texture_srgb");
      auto &e_gamut_index = info.get_resource<std::vector<uint>>("viewport_input_vert", "selection");

      // Compute viewport size minus ImGui's tab bars etc
      // (Re-)create viewport texture if necessary; attached framebuffers are resized separately
      eig::Array2f viewport_size = static_cast<eig::Array2f>(ImGui::GetWindowContentRegionMax())
                                 - static_cast<eig::Array2f>(ImGui::GetWindowContentRegionMin());
      eig::Array2f texture_size  = viewport_size.x();
      if (!i_draw_texture.is_init() || (i_draw_texture.size() != viewport_size.cast<uint>()).all()) {
        i_draw_texture      = {{ .size = texture_size.cast<uint>() }};
        i_draw_texture_srgb = {{ .size = texture_size.cast<uint>() }};
      }

      // Insert image, applying viewport texture to viewport; texture can be safely drawn 
      // to later in the render loop. Flip y-axis UVs to obtain the correct orientation.
      ImGui::Image(ImGui::to_ptr(i_draw_texture_srgb.object()), texture_size, eig::Vector2f(0, 1), eig::Vector2f(1, 0));
      
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
      auto &i_texture = info.get_resource<gl::Texture2d4f>("draw_texture");

      // Update camera info: aspect ratio, scroll delta, move delta
      i_arcball.m_aspect = i_texture.size().x() / i_texture.size().y();
      i_arcball.set_dist_delta(io.MouseWheel);
      if (io.MouseDown[2] || (io.MouseDown[0] && io.KeyCtrl))
        i_arcball.set_pos_delta(eig::Array2f(io.MouseDelta) / i_texture.size().cast<float>());
      i_arcball.update_matrices();
    }

    void eval_gizmo(detail::TaskEvalInfo &info) {
      met_trace_full();

      // Get shared resources
      auto &e_selection = info.get_resource<std::vector<uint>>("viewport_input_vert", "selection")[0];
      auto &e_ocs_centr = info.get_resource<std::vector<Colr>>("gen_color_solids", "ocs_centers")[e_selection];
      auto &i_arcball   = info.get_resource<detail::Arcball>("arcball");
      auto &e_app_data  = info.get_resource<ApplicationData>(global_key, "app_data");
      auto &e_proj_data = e_app_data.project_data;
      auto &e_vert      = e_proj_data.gamut_colr_i;
      auto &e_offs      = e_proj_data.gamut_offs_j;

      // Anchor position is colr + offset, minus center offset 
      eig::Vector3f trf_trnsl = e_vert[e_selection] + e_offs[e_selection] - e_ocs_centr;
      eig::Affine3f trf_basic = eig::Affine3f(eig::Translation3f(trf_trnsl));
      eig::Affine3f trf_delta = eig::Affine3f::Identity();

      // Insert ImGuizmo manipulator at anchor position
      eig::Vector2f rmin = ImGui::GetItemRectMin(), rmax = ImGui::GetItemRectSize();
      ImGuizmo::SetRect(rmin.x(), rmin.y(), rmax.x(), rmax.y());
      ImGuizmo::SetDrawlist(ImGui::GetWindowDrawList());
      ImGuizmo::Manipulate(i_arcball.view().data(), i_arcball.proj().data(), 
        ImGuizmo::OPERATION::TRANSLATE, ImGuizmo::MODE::LOCAL, trf_basic.data(), trf_delta.data());

      // Register gizmo use start, cache current position
      if (ImGuizmo::IsUsing() && !m_is_gizmo_used) {
        m_offs_prev = e_offs;
        m_is_gizmo_used = true;
      }

      // Register gizmo use
      if (ImGuizmo::IsUsing())
        e_offs[e_selection] = (trf_delta * e_offs[e_selection].matrix()).array();

      // Register gizmo use end, update positions
      if (!ImGuizmo::IsUsing() && m_is_gizmo_used) {
        m_is_gizmo_used = false;
        
        // Register data edit as drag finishes
        e_app_data.touch({ 
          .name = "Move gamut offsets", 
          .redo = [edit = e_offs](auto &data) { data.gamut_offs_j = edit; }, 
          .undo = [edit = m_offs_prev](auto &data) { data.gamut_offs_j = edit; }
        });
      }
    }
  };
} // namespace met