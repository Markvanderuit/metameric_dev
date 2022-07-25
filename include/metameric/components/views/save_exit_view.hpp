#pragma once

#include <metameric/core/scheduler.hpp>
#include <metameric/core/state.hpp>
#include <metameric/components/views/detail/file_dialog.hpp>
#include <metameric/components/views/detail/imgui.hpp>
#include <small_gl/window.hpp>
#include <string>

namespace met {
  class SaveExitView : public detail::AbstractTask {
    std::string m_view_title;
    
    void handle_exit(detail::TaskEvalInfo &info) {
      auto &window = info.get_resource<gl::Window>("global", "window");
      window.set_should_close();
    }
    
    void handle_save(detail::TaskEvalInfo &info) {
      auto &app_data = info.get_resource<ApplicationData>("global", "application_data");
      if (!app_data.project_path.empty()) {
        app_data.save(app_data.project_path);
        handle_exit(info);
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
        handle_exit(info);
      }
    }
    
  public:
    SaveExitView(const std::string &name, const std::string &view_title)
    : detail::AbstractTask(name),
      m_view_title(view_title) { }

    void eval(detail::TaskEvalInfo &info) override {
      if (ImGui::BeginPopupModal(m_view_title.c_str(), nullptr, ImGuiWindowFlags_AlwaysAutoResize)) {
        ImGui::TextWrapped("You have unsaved progress. Are you sure you wish to exit Metameric?");
        
        ImGui::Spacing(); ImGui::Separator(); ImGui::Spacing();

        if (ImGui::Button("Exit and save"))       { handle_save(info);          } ImGui::SameLine();
        if (ImGui::Button("Exit without saving")) { handle_exit(info);          } ImGui::SameLine();
        if (ImGui::Button("Cancel"))              { ImGui::CloseCurrentPopup(); }

        ImGui::EndPopup();
      }
    }
  };
} // namespace met