#pragma once

#include <metameric/core/scheduler.hpp>
#include <metameric/core/state.hpp>
#include <metameric/components/views/detail/file_dialog.hpp>
#include <metameric/components/views/detail/imgui.hpp>
#include <metameric/components/schedule.hpp>
#include <string>

namespace met {
  class SaveCloseView : public detail::AbstractTask {
    std::string m_view_title;
    
    void handle_close(detail::TaskEvalInfo &info) {
      // Empty application data as project is closed
      auto &app_data = info.get_resource<ApplicationData>("global", "application_data");
      app_data.clear();
      
      // Currently in a popup's scope; close it
      ImGui::CloseCurrentPopup();

      // Signal schedule re-creation and submit new schedule for main view
      info.signal_flags = detail::TaskSignalFlags::eClearTasks;
      submit_schedule_empty(info);
    }
    
    void handle_save(detail::TaskEvalInfo &info) {
      auto &app_data = info.get_resource<ApplicationData>("global", "application_data");
      if (!app_data.project_path.empty()) {
        app_data.save(app_data.project_path);
        handle_close(info);
      } else {
        handle_save_as(info);
      }
    }

    void handle_save_as(detail::TaskEvalInfo &info) {
      // Open a file picker
      std::filesystem::path path;
      if (detail::save_file_dialog(path, "json") == detail::FileDialogResultType::eOkay) {
        auto &app_data = info.get_resource<ApplicationData>("global", "application_data");
        app_data.save(path.replace_extension(".json"));
        handle_close(info);
      }
    }

  public:
    SaveCloseView(const std::string &name, const std::string &view_title)
    : detail::AbstractTask(name),
      m_view_title(view_title) { }

    void eval(detail::TaskEvalInfo &info) override {
      if (ImGui::BeginPopupModal(m_view_title.c_str(), nullptr, ImGuiWindowFlags_AlwaysAutoResize)) {
        ImGui::TextWrapped("Are you sure you wish to close the project? You may lose unsaved progress.");

        ImGui::Spacing(); ImGui::Separator(); ImGui::Spacing();

        if (ImGui::Button("Close and save"))       { handle_save(info);          } ImGui::SameLine();
        if (ImGui::Button("Close without saving")) { handle_close(info);         } ImGui::SameLine();
        if (ImGui::Button("Cancel"))               { ImGui::CloseCurrentPopup(); }

        ImGui::EndPopup();
      }
    }
  };
} // namespace met