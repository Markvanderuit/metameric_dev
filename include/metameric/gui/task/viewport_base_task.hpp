#pragma once

#include <metameric/core/utility.hpp>
#include <metameric/gui/detail/imgui.hpp>
#include <metameric/gui/detail/linear_scheduler/task.hpp>

namespace met {
  class ViewportBaseTask : public detail::AbstractTask {
  public:
    ViewportBaseTask(const std::string &name)
    : detail::AbstractTask(name) { }

    void init(detail::TaskInitInfo &info) override { }

    void eval(detail::TaskEvalInfo &info) override {
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
  };
} // namespace met