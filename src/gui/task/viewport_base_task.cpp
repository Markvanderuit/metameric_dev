#include <metameric/core/utility.hpp>
#include <metameric/gui/detail/imgui.hpp>
#include <metameric/gui/task/viewport_base_task.hpp>

namespace met {
  ViewportBaseTask::ViewportBaseTask(const std::string &name)
  : detail::AbstractTask(name) { }

  void ViewportBaseTask::init(detail::TaskInitInfo &info) {

  }

  void ViewportBaseTask::eval(detail::TaskEvalInfo &info) {
    if (ImGui::BeginMainMenuBar()) {
      if (ImGui::BeginMenu("File")) {
        // ...
        ImGui::EndMenu();
      }

      if (ImGui::BeginMenu("Edit")) {
        // ...
        ImGui::EndMenu();
      }

      if (ImGui::BeginMenu("View")) {
        // ...
        ImGui::EndMenu();
      }

      if (ImGui::BeginMenu("Help")) {
        // ...
        ImGui::EndMenu();
      }

      ImGui::EndMainMenuBar();
    }
    
    auto flags = ImGuiDockNodeFlags_PassthruCentralNode;
    // auto flags = ImGuiDockNodeFlags_AutoHideTabBar  
    //            | ImGuiDockNodeFlags_PassthruCentralNode;
    ImGui::DockSpaceOverViewport(ImGui::GetMainViewport(), flags);
  }
} // namespace met