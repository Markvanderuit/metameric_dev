#pragma once

#include <metameric/core/state.hpp>
#include <metameric/core/scheduler.hpp>
#include <metameric/components/views/detail/file_dialog.hpp>
#include <metameric/components/views/detail/imgui.hpp>
#include <string>

namespace met {
  class CreateProjectTask : public detail::AbstractTask {
    std::string m_input_path;
    std::string m_progr_path;
    std::string m_view_title;

    void insert_progress_warning(detail::TaskEvalInfo &info) {
      if (ImGui::BeginPopupModal("Warning: unsaved progress")) {
        ImGui::Text("If you continue, you may lose unsaved progress.");
        ImGui::SpacedSeparator();
        if (ImGui::Button("Continue")) {
          create_project(info);
          ImGui::CloseCurrentPopup();
        }
        ImGui::SameLine();
        if (ImGui::Button("Cancel")) { ImGui::CloseCurrentPopup(); }
      }
    }
    
    void insert_file_warning() {
      if (ImGui::BeginPopup("Warning: file not found", 0)) {
        ImGui::Text("The following file could not be found: %s", m_input_path.c_str());
        ImGui::SpacedSeparator();
        if (ImGui::Button("Continue")) { ImGui::CloseCurrentPopup(); }
        ImGui::EndPopup();
      }
    }

    bool create_project_safe(detail::TaskEvalInfo &info) {
      auto &e_app_data = info.get_resource<ApplicationData>(global_key, "app_data");
      if (e_app_data.project_state == ProjectState::eUnsaved 
       || e_app_data.project_state == ProjectState::eNew) {
        ImGui::OpenPopup("Warning: unsaved progress", 0);
        return false;
      } if (!fs::exists(m_input_path)) {
        ImGui::OpenPopup("Warning: file not found", 0);
        return false;
      } else {
        return create_project(info);
      }
    }

    bool create_project(detail::TaskEvalInfo &info) {
      // Create a new project
      info.get_resource<ApplicationData>(global_key, "app_data").create(m_input_path);

      // Signal schedule re-creation and submit new task schedule
      info.signal_flags = detail::TaskSignalFlags::eClearTasks;
      submit_schedule_main(info);

      return true;
    }

  public:
    CreateProjectTask(const std::string &name, const std::string &view_title)
    : detail::AbstractTask(name),
      m_view_title(view_title) { }

    void eval(detail::TaskEvalInfo &info) override {
      if (ImGui::BeginPopupModal(m_view_title.c_str(), nullptr, ImGuiWindowFlags_AlwaysAutoResize)) {
        // Define text input to obtain path and
        // simple '...' button for file selection to obtain path
        ImGui::Text("Path to input texture...");
        ImGui::InputText("##NewProjectPathInputs", &m_input_path);
        ImGui::SameLine();
        if (fs::path path; ImGui::Button("...") && detail::load_dialog(path)) {
          m_input_path = path.string();
        }

        ImGui::SpacedSeparator();

        // Define create/cancel buttons to handle results 
        if (ImGui::Button("Create") && create_project_safe(info)) { ImGui::CloseAnyPopupIfOpen(); }
        ImGui::SameLine();      
        if (ImGui::Button("Cancel")) { ImGui::CloseCurrentPopup(); }

        // Insert modals
        insert_file_warning();
        insert_progress_warning(info);

        ImGui::EndPopup();
      } else {
        // Clear window data when not shown
        m_input_path.clear();
      }
    }
  };
} // namespace met