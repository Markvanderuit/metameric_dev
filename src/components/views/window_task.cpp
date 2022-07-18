#include <metameric/core/utility.hpp>
#include <metameric/components/views/window_task.hpp>
#include <metameric/components/views/detail/imgui.hpp>

namespace met {
  WindowTask::WindowTask(const std::string &name)
  : detail::AbstractTask(name) { }

  void WindowTask::eval(detail::TaskEvalInfo &info) {
    // Set up the menu bar at the top of the window's viewport
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
    
    // Create an explicit dock space over the entire window's viewport, excluding the menu bar
    ImGui::DockSpaceOverViewport(ImGui::GetMainViewport(), ImGuiDockNodeFlags_PassthruCentralNode);
  }
} // namespace met