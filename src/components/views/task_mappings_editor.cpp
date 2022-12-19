#include <metameric/core/math.hpp>
#include <metameric/core/detail/trace.hpp>
#include <metameric/components/views/task_mappings_editor.hpp>
#include <metameric/components/views/detail/imgui.hpp>

namespace met {
  constexpr float list_width_relative  = 0.45f;
  constexpr float list_width_max       = 150.f;
  constexpr float select_right_padding = -32.f;
  const static ImVec2      add_button_size       = { 16.f, 16.f };
  const static std::string default_mapping_title = "mapping_";
  
  void MappingsEditorTask::add_mapping(detail::TaskEvalInfo &info) {
    met_trace_full();

    // Get external shared resources
    auto &e_appl_data = info.get_resource<ApplicationData>(global_key, "app_data");
    auto &e_mappings = e_appl_data.project_data.mappings;

    // Make a copy of the mapping data and add a new default mapping
    auto cp_mappings = e_mappings;
    cp_mappings.push_back({ .cmfs = 0, .illuminant = 0 });

    // Register data edit
    e_appl_data.touch({ 
      .name = "Add mapping",
      .redo = [&e_appl_data, edit = cp_mappings](auto &data) { data.mappings = edit; }, 
      .undo = [&e_appl_data, edit = e_mappings](auto &data) { data.mappings = edit; }
    });

    // Set selection to newly added item
    m_selected_i         = e_mappings.size() - 1;
    m_selected_mapping   = e_mappings[m_selected_i];
  }

  void MappingsEditorTask::remove_mapping(detail::TaskEvalInfo &info) {
    met_trace_full();

    // Get external shared resources
    auto &e_appl_data = info.get_resource<ApplicationData>(global_key, "app_data");
    auto &e_mappings = e_appl_data.project_data.mappings;

    // Make a copy of the mapping data and remove the current selected mapping
    auto cp_mappings = e_mappings;
    cp_mappings.erase(cp_mappings.begin() + m_selected_i);

    // Register data edit
    e_appl_data.touch({ 
      .name = "Remove mapping",
      .redo = [&e_appl_data, edit = cp_mappings](auto &data) { data.mappings = edit; }, 
      .undo = [&e_appl_data, edit = e_mappings](auto &data) { data.mappings = edit; }
    });

    // Clear selection
    m_selected_i       = -1;
    m_selected_mapping = {};
  }

  void MappingsEditorTask::change_mapping(detail::TaskEvalInfo &info) {
    met_trace_full();

    // Get external shared resources
    auto &e_appl_data = info.get_resource<ApplicationData>(global_key, "app_data");
    auto &e_mappings = e_appl_data.project_data.mappings;

    // Define data before/after edit
    auto redo = m_selected_mapping;
    auto undo = e_mappings[m_selected_i];

    // Register data edit
    e_appl_data.touch({ 
      .name = "Change mapping",
      .redo = [&e_appl_data, i = m_selected_i, edit = redo](auto &data) { data.mappings[i] = edit; },
      .undo = [&e_appl_data, i = m_selected_i, edit = undo](auto &data) { data.mappings[i] = edit; }
    });
  }
  
  void MappingsEditorTask::reset_mapping(detail::TaskEvalInfo &info) {
    met_trace_full();

    // Get external shared resources
    auto &e_mappings = info.get_resource<ApplicationData>(global_key, "app_data").project_data.mappings;

    // Reset to stored data of selected mapping
    m_selected_mapping  = e_mappings[m_selected_i];
  }

  void MappingsEditorTask::draw_list(detail::TaskEvalInfo &info) {
    met_trace_full();

    // Get external shared resources
    auto &e_appl_data = info.get_resource<ApplicationData>(global_key, "app_data");
    auto &e_proj_data = e_appl_data.project_data;
    auto &e_mappings  = e_proj_data.mappings;

    // Content area width determines list width
    float window_width = ImGui::GetContentRegionAvail().x;
    float list_width   = std::min(list_width_relative * window_width, list_width_max);

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
        if (ImGui::Selectable(e_proj_data.mapping_name(i).c_str(), m_selected_i == i)) {
          // Apply selection
          m_selected_i       = i;
          m_selected_mapping = e_mappings[i];
        }
      }
      ImGui::EndListBox();
    }

    // Add-mapping button and remove-mapping button; show remove only if a
    // mapping is selected and n_mappings > 1
    const bool show_remove   = m_selected_i != -1 && e_mappings.size() > 1;
    if (ImGui::Button("+")) { add_mapping(info); }
    ImGui::SameLine();
    if (show_remove && ImGui::Button("-")) { remove_mapping(info); }

    // End list draw group
    ImGui::PopItemWidth();
    ImGui::EndGroup();
  }

  void MappingsEditorTask::draw_selection(detail::TaskEvalInfo &info) {
    met_trace_full();

    // Get external shared resources
    auto &e_appl_data   = info.get_resource<ApplicationData>(global_key, "app_data");
    auto &e_proj_data   = e_appl_data.project_data;
    auto &e_mappings    = e_proj_data.mappings;
    auto &e_cmfs        = e_proj_data.cmfs;
    auto &e_illuminants = e_proj_data.illuminants;

    // Content area width determines selection width as remaining space
    float window_width = ImGui::GetContentRegionAvail().x;
    float list_width   = std::min(list_width_relative * window_width, list_width_max);

    // Begin selection draw group
    ImGui::BeginGroup();
    ImGui::PushItemWidth(-list_width - select_right_padding);

    // Draw edit title, properly aligned 
    ImGui::AlignTextToFramePadding();
    ImGui::Text("Edit selected");
    ImGui::Spacing();

    // Draw CMFS selector widget
    if (ImGui::BeginCombo("CMFS", e_cmfs[m_selected_mapping.cmfs].first.c_str())) {
      for (uint i = 0; i < e_cmfs.size(); ++i) {
        if (ImGui::Selectable(e_cmfs[i].first.c_str(), m_selected_mapping.cmfs == i)) {
          m_selected_mapping.cmfs = i;
        }
      }
      ImGui::EndCombo();
    }

    // Draw illuminant selector widget
    if (ImGui::BeginCombo("Illuminant", e_illuminants[m_selected_mapping.illuminant].first.c_str())) {
      for (uint i = 0; i < e_illuminants.size(); ++i) {
        if (ImGui::Selectable(e_illuminants[i].first.c_str(), m_selected_mapping.illuminant == i)) {
          m_selected_mapping.illuminant = i;
        }
      }
      ImGui::EndCombo();
    }

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
    met_trace_full();

    m_selected_i = -1;

    // Share a selection key that can be set from the outside, and is then reset, so
    // other parts of the program can influence selection
    info.insert_resource<int>("selected_i", -1);
  }

  void MappingsEditorTask::eval(detail::TaskEvalInfo &info) {
    met_trace_full();

    if (ImGui::Begin("Mappings editor")) {      
      // Get shared resources
      auto &e_appl_data = info.get_resource<ApplicationData>(global_key, "app_data");
      auto &e_proj_data = e_appl_data.project_data;
      auto &selected_i  = info.get_resource<int>("selected_i");

      // Handle external selection key
      if (selected_i != -1 && selected_i < e_proj_data.mappings.size()) {
        m_selected_i = selected_i;
      }
      selected_i = -1;

      draw_list(info);
      if (m_selected_i != -1) {
        ImGui::SameLine();
        draw_selection(info);
      }
    }
    ImGui::End();
  }
} // namespace met 