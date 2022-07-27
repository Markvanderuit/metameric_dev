#include <metameric/core/state.hpp>
#include <metameric/components/views/mapping_viewer.hpp>
#include <metameric/components/views/detail/imgui.hpp>

namespace met {
  constexpr float  list_width           = 150.f;
  constexpr float  select_right_padding = 16.f;
  const static ImVec2      add_button_size       = { 28.f, 28.f };
  const static std::string default_mapping_title = "mapping_";
  
  void MappingViewer::add_mapping(detail::TaskEvalInfo &info) {
    // Get external shared resources
    auto &e_app_data = info.get_resource<ApplicationData>(global_key, "app_data");
    auto &e_mappings = e_app_data.project_data.loaded_mappings;

    // Make a copy of the mapping data and add a new default mapping
    auto cp_mappings = e_mappings;
    cp_mappings.push_back({ default_mapping_title + std::to_string(cp_mappings.size()), {
      .cmfs       = "CIE XYZ->sRGB", .illuminant = "D65", .n_scatters = 0 
    }});

    // Register data edit
    e_app_data.touch({ 
      .name = "Add mapping",
      .redo = [edit = cp_mappings](auto &data) { data.loaded_mappings = edit; }, 
      .undo = [edit = e_mappings](auto &data) { data.loaded_mappings = edit; }
    });

    // Set selection to newly added item
    m_selected_i         = e_mappings.size() - 1;
    auto &[key, mapping] = e_mappings[m_selected_i];
    m_selected_key       = key;
    m_selected_mapping   = mapping;
  }

  void MappingViewer::remove_mapping(detail::TaskEvalInfo &info) {
    // Get external shared resources
    auto &e_app_data = info.get_resource<ApplicationData>(global_key, "app_data");
    auto &e_mappings = e_app_data.project_data.loaded_mappings;

    // Make a copy of the mapping data and remove the current selected mapping
    auto cp_mappings = e_mappings;
    cp_mappings.erase(cp_mappings.begin() + m_selected_i);

    // Register data edit
    e_app_data.touch({ 
      .name = "Remove mapping",
      .redo = [edit = cp_mappings](auto &data) { data.loaded_mappings = edit; }, 
      .undo = [edit = e_mappings](auto &data) { data.loaded_mappings = edit; }
    });

    // Clear selection
    m_selected_i       = -1;
    m_selected_key     = "";
    m_selected_mapping = {};
  }

  void MappingViewer::change_mapping(detail::TaskEvalInfo &info) {
    // Get external shared resources
    auto &e_app_data = info.get_resource<ApplicationData>(global_key, "app_data");
    auto &e_mappings = e_app_data.project_data.loaded_mappings;

    // Define data before/after edit
    auto redo_pair = std::pair<std::string, MappingData> { m_selected_key, m_selected_mapping };
    auto undo_pair = e_mappings[m_selected_i];
    
    // Register data edit
    e_app_data.touch({ 
      .name = "Change mapping",
      .redo = [i = m_selected_i, edit = redo_pair](auto &data) { data.loaded_mappings[i] = edit; },
      .undo = [i = m_selected_i, edit = undo_pair](auto &data) { data.loaded_mappings[i] = edit; }
    });
  }
  
  void MappingViewer::reset_mapping(detail::TaskEvalInfo &info) {
    // Get external shared resources
    auto &e_mappings = info.get_resource<ApplicationData>(global_key, "app_data").project_data.loaded_mappings;

    // Reset to stored data of selected mapping
    auto &[key, mapping] = e_mappings[m_selected_i];
    m_selected_key      = key;
    m_selected_mapping  = mapping;
  }

  void MappingViewer::draw_list(detail::TaskEvalInfo &info) {
    // Get external shared resources
    auto &e_app_data = info.get_resource<ApplicationData>(global_key, "app_data");
    auto &e_mappings = e_app_data.project_data.loaded_mappings;

    // Begin list draw group
    ImGui::BeginGroup();
    ImGui::PushItemWidth(list_width);

    // Draw list title, properly aligned 
    ImGui::AlignTextToFramePadding();
    ImGui::Text("Mappings");
    ImGui::Separator();

    // Draw list box for selecting a mapping, box label is hidden
    if (ImGui::BeginListBox("##SpectralMappingsListBox", { list_width, 0.f })) {
      for (uint i = 0; i < e_mappings.size(); ++i) {
        auto &[key, mapping] = e_mappings[i];
        if (ImGui::Selectable(key.c_str(), m_selected_i == i)) {
          // Apply selection
          m_selected_i       = i;
          m_selected_key     = key;
          m_selected_mapping = mapping;
        }
      }
      ImGui::EndListBox();
    }

    // Add-mapping button and remove-mapping button; show remove only if a
    // mapping is selected and n_mappings > 1
    const bool show_remove   = m_selected_i != -1 && e_mappings.size() > 1;
    if (ImGui::Button("+", add_button_size)) { add_mapping(info); }
    ImGui::SameLine();
    if (show_remove && ImGui::Button("-", add_button_size)) { remove_mapping(info); }

    // End list draw group
    ImGui::PopItemWidth();
    ImGui::EndGroup();
  }

  void MappingViewer::draw_selection(detail::TaskEvalInfo &info) {
    // Get external shared resources
    auto &e_app_data = info.get_resource<ApplicationData>(global_key, "app_data");
    auto &e_prj_data = e_app_data.project_data;
    auto &e_mappings = e_prj_data.loaded_mappings;

    // Begin selection draw group
    ImGui::BeginGroup();
    ImGui::PushItemWidth(-list_width - select_right_padding);

    // Draw edit title, properly aligned 
    ImGui::AlignTextToFramePadding();
    ImGui::Text("Edit selected");
    ImGui::Spacing();

    // Draw mapping name edit widget
    ImGui::InputText("Name", &m_selected_key);

    // Draw CMFS selector widget
    if (ImGui::BeginCombo("CMFS", m_selected_mapping.cmfs.c_str())) {
      for (auto &[key, _] : e_prj_data.loaded_cmfs) {
        if (ImGui::Selectable(key.c_str(), false)) {
          m_selected_mapping.cmfs = key;
        }
      }
      ImGui::EndCombo();
    }

    // Draw illuminant selector widget
    if (ImGui::BeginCombo("Illuminant", m_selected_mapping.illuminant.c_str())) {
      for (auto &[key, _] : e_prj_data.loaded_illuminants) {
        if (ImGui::Selectable(key.c_str(), false)) {
          m_selected_mapping.illuminant = key;
        }
      }
      ImGui::EndCombo();
    }

    // Scattering depth selector slider; between 0 and 8 should suffice?
    uint scatter_min = 0, scatter_max = 8;
    ImGui::SliderScalar("Nr. of scatterings", ImGuiDataType_U32,
      &(m_selected_mapping.n_scatters), &scatter_min, &scatter_max);

    // Draw buttons to apply/reset changes to stored mapping
    if (ImGui::Button("Apply")) { change_mapping(info); }
    ImGui::SameLine();
    if (ImGui::Button("Reset")) { reset_mapping(info); }

    // End selection draw group
    ImGui::PopItemWidth();
    ImGui::EndGroup();
  }

  MappingViewer::MappingViewer(const std::string &name)
  : detail::AbstractTask(name) { }

  void MappingViewer::init(detail::TaskInitInfo &info) {
    m_selected_i = -1;
  }

  void MappingViewer::eval(detail::TaskEvalInfo &info) {
    if (ImGui::Begin("Spectral mappings")) {      
      draw_list(info);
      if (m_selected_i != -1) {
        ImGui::SameLine();
        draw_selection(info);
      }
    }
    ImGui::End();
  }
} // namespace met 