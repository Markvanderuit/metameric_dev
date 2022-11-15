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

    void eval(detail::TaskEvalInfo &info) override {
      met_trace_full();
    
      // Get shared resources
      auto &e_gamut_index = info.get_resource<std::vector<uint>>("viewport_input", "gamut_selection");

      // Only spawn tooltip on non-empty gamut selection
      guard(!e_gamut_index.empty());

      // Compute viewport offset and size, minus ImGui's tab bars etc
      eig::Array2f viewport_offs = static_cast<eig::Array2f>(ImGui::GetWindowPos()) 
                                 + static_cast<eig::Array2f>(ImGui::GetWindowContentRegionMin());

      // Spawn window with selection info if one or more vertices are selected
      eig::Array2f ttip_posi = viewport_offs + 16.f;
      ImGui::SetNextWindowPos(ttip_posi);
      if (ImGui::Begin("Vertex selection", NULL, ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoDocking)) {
        if (e_gamut_index.size() == 1) {
          eval_single(info);
        } else {
          eval_multiple(info);
        }
      }

      // Capture size of window before closing to offset next window properly 
      eig::Array2f ttip_size = static_cast<eig::Array2f>(ImGui::GetWindowSize());
      ImGui::End();

      // Spawn window for metamer set editing if there is one vertex selected
      if (e_gamut_index.size() == 1) {
        ttip_posi.y() += ttip_size.y() + 16.f;
        ttip_size.y() = 384.f;
        ImGui::SetNextWindowPos(ttip_posi);
        ImGui::SetNextWindowSize(ttip_size);
        if (ImGui::Begin("Metamer set", NULL, ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoDocking)) {
          eval_metamer_set(info);
        }
        ImGui::End();
      }
    }

    void eval_single(detail::TaskEvalInfo &info) {
      met_trace_full();

      constexpr 
      auto i_get = [](auto &v) { return [&v](const auto &i) -> auto& { return v[i]; }; };

      // Get shared resources
      auto &e_app_data     = info.get_resource<ApplicationData>(global_key, "app_data");
      auto &e_gamut_spec   = e_app_data.project_data.gamut_spec;
      auto &e_gamut_colr   = e_app_data.project_data.gamut_colr_i;
      auto &e_gamut_offs   = e_app_data.project_data.gamut_offs_j;
      auto &e_gamut_mapp_i = e_app_data.project_data.gamut_mapp_i;
      auto &e_gamut_mapp_j = e_app_data.project_data.gamut_mapp_j;
      auto &e_mappings     = e_app_data.loaded_mappings;
      auto &e_mapping_data = e_app_data.project_data.mappings;
      auto &e_gamut_index  = info.get_resource<std::vector<uint>>("viewport_input", "gamut_selection");

      // Obtain selected reflectance and colors
      Spec &gamut_spec   = e_gamut_spec[e_gamut_index[0]];
      Colr &gamut_colr_i = e_gamut_colr[e_gamut_index[0]];
      Colr &gamut_offs_j = e_gamut_offs[e_gamut_index[0]];

      // Local copies of gamut mapping indices
      uint l_gamut_mapp_i = e_gamut_mapp_i[e_gamut_index[0]];
      uint l_gamut_mapp_j = e_gamut_mapp_j[e_gamut_index[0]];
      
      // Names of selected mappings
      const auto &mapping_name_i = e_mapping_data[l_gamut_mapp_i].first;
      const auto &mapping_name_j = e_mapping_data[l_gamut_mapp_j].first;

      // Compute resulting color and error
      Colr gamut_actual = e_mappings[e_gamut_mapp_i[e_gamut_index[0]]].apply_color(gamut_spec);
      Colr gamut_error  = (gamut_actual - gamut_colr_i).abs();

      // Plot of solved-for reflectance
      ImGui::PlotLines("Reflectance", gamut_spec.data(), gamut_spec.rows(), 0, nullptr, 0.f, 1.f, { 0.f, 64.f });

      ImGui::Separator();

      ImGui::ColorEdit3("Color, coords", gamut_colr_i.data(), ImGuiColorEditFlags_Float);
      ImGui::ColorEdit3("Color, error",  gamut_error.data(), ImGuiColorEditFlags_Float);

      ImGui::Separator();

      // Selector for first mapping index
      if (ImGui::BeginCombo("Mapping 0", mapping_name_i.c_str())) {
        for (uint i = 0; i < e_mapping_data.size(); ++i) {
          auto &[key, _] = e_mapping_data[i];
          if (ImGui::Selectable(key.c_str(), i == l_gamut_mapp_i)) {
            l_gamut_mapp_i = i;
          }
        }
        ImGui::EndCombo();
      }

      // Selector for second mapping index
      if (ImGui::BeginCombo("Mapping 1", mapping_name_j.c_str())) {
        for (uint i = 0; i < e_mapping_data.size(); ++i) {
          auto &[key, _] = e_mapping_data[i];
          if (ImGui::Selectable(key.c_str(), i == l_gamut_mapp_j)) {
            l_gamut_mapp_j = i;
          }
        }
        ImGui::EndCombo();
      }

      // If changes to local copies were made, register a data edit
      if (l_gamut_mapp_i != e_gamut_mapp_i[e_gamut_index[0]]) {
        e_app_data.touch({
          .name = "Change gamut mapping 0",
          .redo = [edit = l_gamut_mapp_i,                   i = e_gamut_index[0]](auto &data) { data.gamut_mapp_i[i] = edit; },
          .undo = [edit = e_gamut_mapp_i[e_gamut_index[0]], i = e_gamut_index[0]](auto &data) { data.gamut_mapp_i[i] = edit; }
        });
      }
      if (l_gamut_mapp_j != e_gamut_mapp_j[e_gamut_index[0]]) {
        e_app_data.touch({
          .name = "Change gamut mapping 1",
          .redo = [edit = l_gamut_mapp_j,                   i = e_gamut_index[0]](auto &data) { data.gamut_mapp_j[i] = edit; },
          .undo = [edit = e_gamut_mapp_j[e_gamut_index[0]], i = e_gamut_index[0]](auto &data) { data.gamut_mapp_j[i] = edit; }
        });
      }
    }

    void eval_multiple(detail::TaskEvalInfo &info) {
      met_trace_full();

      /* constexpr 
      auto i_get = [](auto &v) { return [&v](const auto &i) -> auto& { return v[i]; }; }; */

      // Get shared resources
      auto &e_app_data     = info.get_resource<ApplicationData>(global_key, "app_data");
      auto &e_gamut_spec   = e_app_data.project_data.gamut_spec;
      auto &e_gamut_colr   = e_app_data.project_data.gamut_colr_i;
      auto &e_gamut_offs   = e_app_data.project_data.gamut_offs_j;
      auto &e_gamut_mapp_i = e_app_data.project_data.gamut_mapp_i;
      auto &e_gamut_mapp_j = e_app_data.project_data.gamut_mapp_j;
      auto &e_mappings     = e_app_data.loaded_mappings;
      auto &e_mapping_data = e_app_data.project_data.mappings;
      auto &e_gamut_index  = info.get_resource<std::vector<uint>>("viewport_input", "gamut_selection");

      // Iterate
      for (const uint &i : e_gamut_index) {
        if (ImGui::CollapsingHeader(fmt::format("Vertex {}", i).c_str(), ImGuiTreeNodeFlags_DefaultOpen)) {
          ImGui::Text("Metamer set");

          // Obtain selected reflectance and colors
          Spec &gamut_spec   = e_gamut_spec[i];
          Colr &gamut_colr_i = e_gamut_colr[i];
          Colr &gamut_offs_j = e_gamut_offs[i];

          // Local copies of gamut mapping indices
          uint l_gamut_mapp_i = e_gamut_mapp_i[i];
          uint l_gamut_mapp_j = e_gamut_mapp_j[i];
          
          // Names of selected mappings
          const auto &mapping_name_i = e_mapping_data[l_gamut_mapp_i].first;
          const auto &mapping_name_j = e_mapping_data[l_gamut_mapp_j].first;

          // Compute resulting color and error
          Colr gamut_actual = e_mappings[e_gamut_mapp_i[i]].apply_color(gamut_spec);
          Colr gamut_error  = (gamut_actual - gamut_colr_i).abs();

          // Plot of solved-for reflectance
          ImGui::PlotLines("Reflectance", gamut_spec.data(), gamut_spec.rows(), 0, nullptr, 0.f, 1.f, { 0.f, 64.f });

          ImGui::Separator();

          ImGui::ColorEdit3("Color, coords", gamut_colr_i.data(), ImGuiColorEditFlags_Float);
          ImGui::ColorEdit3("Color, error",  gamut_error.data(), ImGuiColorEditFlags_Float);
        }
      }
    }

    void eval_metamer_set(detail::TaskEvalInfo &info) {
      ImGui::Text("Metamer set");
    }
  };
} // namespace met