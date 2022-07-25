#pragma once

#include <metameric/core/state.hpp>
#include <metameric/core/scheduler.hpp>
#include <metameric/components/views/detail/file_dialog.hpp>
#include <metameric/components/views/detail/imgui.hpp>
#include <string>

namespace met {
  class NewProjectView : public detail::AbstractTask {
    std::string m_input_path;
    std::string m_progr_path;
    std::string m_view_title;

    void insert_progress_warning(detail::TaskEvalInfo &info) {
      if (ImGui::BeginPopupModal("Warning: unsaved progress")) {
        ImGui::Text("If you continue, you may lose unsaved progress.");
        ImGui::Separator();
        if (ImGui::Button("Continue")) {
          create_project(info);
          ImGui::CloseCurrentPopup();
        }
        if (ImGui::Button("Cancel")) {
          ImGui::CloseCurrentPopup();
        }
      }
    }
    
    void insert_file_warning() {
      if (ImGui::BeginPopup("Warning: file not found", 0)) {
        ImGui::Text("The following file could not be found: %s", m_input_path.c_str());
        ImGui::Separator();
        if (ImGui::Button("Continue")) {
          ImGui::CloseCurrentPopup();
        }
        ImGui::EndPopup();
      }
    }

    void call_progress_warning() {
      ImGui::OpenPopup("Warning: unsaved progress", 0);
    }

    void call_file_warning() {
      ImGui::OpenPopup("Warning: file not found", 0);
    }
    
    bool create_project_safe(detail::TaskEvalInfo &info) {
      auto &app_data = info.get_resource<ApplicationData>("global", "application_data");
      if (app_data.project_state == ProjectState::eModified 
       || app_data.project_state == ProjectState::eUnsaved) {
        call_progress_warning();
        return false;
      } if (!std::filesystem::exists(m_input_path)) {
        call_file_warning();
        return false;
      } else {
        return create_project(info);
      }
    }

    bool create_project(detail::TaskEvalInfo &info) {
      // Get shared resources
      auto &app_data = info.get_resource<ApplicationData>("global", "application_data");

      // Initialize new unsaved project
      app_data.project_state = ProjectState::eUnsaved;
      app_data.project_data  = ProjectData();
      app_data.project_path  = "";

      // Load linearized rgb texture into application
      app_data.rgb_texture = Texture2d3f {{ .path = m_input_path }};
      // app_data.rgb_texture = io::as_lrgb(Texture2d3f {{ .path = m_input_path }});

      // Signal schedule re-creation and submit new task schedule
      info.signal_flags = detail::TaskSignalFlags::eClearTasks;
      submit_schedule_main(info);

      return true;
    }

  public:
    NewProjectView(const std::string &name, const std::string &view_title)
    : detail::AbstractTask(name),
      m_view_title(view_title) { }

    void eval(detail::TaskEvalInfo &info) override {
      if (ImGui::BeginPopupModal(m_view_title.c_str(), nullptr, ImGuiWindowFlags_AlwaysAutoResize)) {
        ImGui::Text("Path to input texture...");
        ImGui::InputText("##NewProjectPathInputs", &m_input_path);
        ImGui::SameLine();

        if (ImGui::Button("...")) {
          std::filesystem::path path;
          if (detail::open_file_dialog(path) == detail::FileDialogResultType::eOkay) {
            m_input_path = path.string();
          }
        }

        ImGui::Spacing();
        ImGui::Separator();
        ImGui::Spacing();

        if (ImGui::Button("Create")) {
          if (create_project_safe(info)) {
            ImGui::CloseCurrentPopup();
          }
        }
        
        ImGui::SameLine();      
        
        if (ImGui::Button("Cancel")) {
          ImGui::CloseCurrentPopup();
        }

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