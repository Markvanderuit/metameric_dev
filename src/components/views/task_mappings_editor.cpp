#include <metameric/components/views/task_mappings_editor.hpp>
#include <metameric/components/views/detail/imgui.hpp>

namespace met {
  constexpr float  list_width           = 150.f;
  constexpr float  select_right_padding = 16.f;
  const static ImVec2      add_button_size       = { 28.f, 28.f };
  const static std::string default_mapping_title = "mapping_";
  
  void MappingsEditorTask::add_mapping(detail::TaskEvalInfo &info) {
    // Get external shared resources
    auto &e_app_data = info.get_resource<ApplicationData>(global_key, "app_data");
    auto &e_mappings = e_app_data.project_data.loaded_mappings;

    // Make a copy of the mapping data and add a new default mapping
    auto cp_mappings = e_mappings;
    cp_mappings.push_back({ default_mapping_title + std::to_string(cp_mappings.size()), {
      .cmfs = "CIE XYZ->sRGB", .illuminant = "D65", .n_scatters = 0 
    }});

    // Register data edit
    e_app_data.touch({ 
      .name = "Add mapping",
      .redo = [&e_app_data, edit = cp_mappings](auto &data) { 
        data.loaded_mappings = edit; 
        e_app_data.load_mappings();
      }, 
      .undo = [&e_app_data, edit = e_mappings](auto &data) { 
        data.loaded_mappings = edit; 
        e_app_data.load_mappings();
      }
    });

    // Set selection to newly added item
    m_selected_i         = e_mappings.size() - 1;
    auto &[key, mapping] = e_mappings[m_selected_i];
    m_selected_key       = key;
    m_selected_mapping   = mapping;
  }

  void MappingsEditorTask::remove_mapping(detail::TaskEvalInfo &info) {
    // Get external shared resources
    auto &e_app_data = info.get_resource<ApplicationData>(global_key, "app_data");
    auto &e_mappings = e_app_data.project_data.loaded_mappings;

    // Make a copy of the mapping data and remove the current selected mapping
    auto cp_mappings = e_mappings;
    cp_mappings.erase(cp_mappings.begin() + m_selected_i);

    // Register data edit
    e_app_data.touch({ 
      .name = "Remove mapping",
      .redo = [&e_app_data, edit = cp_mappings](auto &data) {
        data.loaded_mappings = edit;
        e_app_data.load_mappings();
      }, 
      .undo = [&e_app_data, edit = e_mappings](auto &data) {
        data.loaded_mappings = edit;
        e_app_data.load_mappings();
      }
    });

    // Clear selection
    m_selected_i       = -1;
    m_selected_key     = "";
    m_selected_mapping = {};
  }

  void MappingsEditorTask::change_mapping(detail::TaskEvalInfo &info) {
    // Get external shared resources
    auto &e_app_data = info.get_resource<ApplicationData>(global_key, "app_data");
    auto &e_mappings = e_app_data.project_data.loaded_mappings;

    // Define data before/after edit
    auto redo_pair = std::pair<std::string, MappingData> { m_selected_key, m_selected_mapping };
    auto undo_pair = e_mappings[m_selected_i];

    // Register data edit
    e_app_data.touch({ 
      .name = "Change mapping",
      .redo = [&e_app_data, i = m_selected_i, edit = redo_pair](auto &data) { 
        data.loaded_mappings[i] = edit;
        e_app_data.load_mappings(); 
      },
      .undo = [&e_app_data, i = m_selected_i, edit = undo_pair](auto &data) { 
        data.loaded_mappings[i] = edit;
        e_app_data.load_mappings(); 
      }
    });
  }
  
  void MappingsEditorTask::reset_mapping(detail::TaskEvalInfo &info) {
    // Get external shared resources
    auto &e_mappings = info.get_resource<ApplicationData>(global_key, "app_data").project_data.loaded_mappings;

    // Reset to stored data of selected mapping
    auto &[key, mapping] = e_mappings[m_selected_i];
    m_selected_key      = key;
    m_selected_mapping  = mapping;
  }

  void MappingsEditorTask::draw_list(detail::TaskEvalInfo &info) {
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

  void MappingsEditorTask::draw_selection(detail::TaskEvalInfo &info) {
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

  MappingsEditorTask::MappingsEditorTask(const std::string &name)
  : detail::AbstractTask(name) { }

  void MappingsEditorTask::init(detail::TaskInitInfo &info) {
    m_selected_i = -1;
  }

  void MappingsEditorTask::eval(detail::TaskEvalInfo &info) {
    if (ImGui::Begin("Mappings editor")) {      
      draw_list(info);
      if (m_selected_i != -1) {
        ImGui::SameLine();
        draw_selection(info);
      }
    }
    ImGui::End();
  }
} // namespace met 