#include <metameric/core/state.hpp>
#include <metameric/components/views/mapping_viewer.hpp>
#include <metameric/components/views/detail/imgui.hpp>

namespace met {
  MappingViewer::MappingViewer(const std::string &name)
  : detail::AbstractTask(name) { }

  void MappingViewer::init(detail::TaskInitInfo &info) {
    m_selected_mapping_i = 0;
    // TODO delete if unnecessary
  }

  void MappingViewer::eval(detail::TaskEvalInfo &info) {
    auto &e_app_data = info.get_resource<ApplicationData>(global_key, "app_data");
    auto &e_prj_data = e_app_data.project_data;
    auto &e_mappings = e_prj_data.loaded_mappings;

    if (ImGui::Begin("Spectral mappings")) {
      auto viewport_size = static_cast<glm::vec2>(ImGui::GetWindowContentRegionMax())
                         - static_cast<glm::vec2>(ImGui::GetWindowContentRegionMin());
      float window_width = viewport_size.x;
      float list_width   = 96.f;
      float list_padd    = 12.f;
      float graph_width  = 192.f;
      float selc_width   = std::max(0.f, window_width - list_width - graph_width - 2 * list_padd);
      
      ImGui::BeginGroup();

      ImGui::AlignTextToFramePadding();
      ImGui::Text("Mappings");
      ImGui::SpacedSeparator();

      if (ImGui::BeginListBox("##SpectralMappingsListBox", ImVec2(list_width, 0.f))) {
        for (uint i = 0; i < e_mappings.size(); ++i) {
          auto &[key, mapping] = e_mappings[i];
          if (ImGui::Selectable(key.c_str(), m_selected_mapping_i == i)) {
            m_selected_mapping_i    = i;
            m_selected_mapping_edit = mapping;
          }
        }
        ImGui::EndListBox();
      }

      ImGui::SpacedSeparator();
      if (ImGui::Button("Add")) {
        // ...
      }
      ImGui::SameLine();
      if (ImGui::Button("Remove")) {
        // ...
      }

      // ImGui::PopItemWidth();
      ImGui::EndGroup();

      ImGui::SameLine();
      
      ImGui::BeginGroup();

      // ImGui::AlignTextToFramePadding();
      // ImGui::Text("Selected mapping");

      ImGui::PushItemWidth(-list_width);

      auto &[sel_key, sel_mapping] = e_mappings[m_selected_mapping_i];
      // auto &[selected_mapping_key, _] = e_mappings[m_selected_mapping_i]

      ImGui::InputText("Selected mapping", &m_selected_mapping_key);
      ImGui::Spacing();
      ImGui::Spacing();
      ImGui::Spacing();

      // CMFS selector widget
      if (ImGui::BeginCombo("CMFS", sel_mapping.cmfs.c_str())) {
        for (auto &[key, _] : e_prj_data.loaded_cmfs) {
          if (ImGui::Selectable(key.c_str(), false)) {
            sel_mapping.cmfs = key;
          }
        }
        ImGui::EndCombo();
      }

      // Illuminant selector widget
      if (ImGui::BeginCombo("Illuminant", sel_mapping.illuminant.c_str())) {
        for (auto &[key, _] : e_prj_data.loaded_illuminants) {
          if (ImGui::Selectable(key.c_str(), false)) {
            sel_mapping.illuminant = key;
          }
        }
        ImGui::EndCombo();
      }

      // Scattering depth selector slider; between 0 and 8 should suffice?
      uint scatter_min = 0, scatter_max = 8;
      ImGui::SliderScalar("Nr. of scatterings", ImGuiDataType_U32,
        &(sel_mapping.n_scatters), &scatter_min, &scatter_max);

      ImGui::PopItemWidth();

      if (ImGui::Button("Apply changes")) {
        // ...
      }

      ImGui::EndGroup();
    }
    ImGui::End();
  }
} // namespace met 