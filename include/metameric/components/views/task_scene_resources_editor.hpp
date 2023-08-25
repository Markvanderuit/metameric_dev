#pragma once

#include <metameric/core/scheduler.hpp>
#include <metameric/core/scene_handler.hpp>
#include <metameric/components/views/detail/imgui.hpp>

namespace met {
  class SceneResourcesEditorTask : public detail::TaskNode {
    
  public:
    void eval(SchedulerHandle &info) override {
      met_trace_full();

      if (ImGui::Begin("Scene resources")) {
        // Get external resources
        const auto &e_handler = info.global("scene_handler").read_only<SceneHandler>();
        const auto &e_scene   = e_handler.scene;

        if (ImGui::CollapsingHeader("Meshes")) {
          for (const auto &mesh : e_scene.meshes) {
            if (ImGui::CollapsingHeader(mesh.name.c_str())) {
              // ...
            }
          }
        }
        if (ImGui::CollapsingHeader("Textures")) {
          for (const auto &txtr : e_scene.textures_3f) {
            if (ImGui::CollapsingHeader(txtr.name.c_str())) {
              // ...
            }
          }
          for (const auto &txtr : e_scene.textures_1f) {
            if (ImGui::CollapsingHeader(txtr.name.c_str())) {
              // ...
            }
          }
        }
        if (ImGui::CollapsingHeader("Illuminant functions")) {
          for (const auto &func : e_scene.illuminants) {
            if (ImGui::CollapsingHeader(func.name.c_str())) {
              // ...
            }
          }
        }
        if (ImGui::CollapsingHeader("Observer functions")) {
          for (const auto &func : e_scene.observers) {
            if (ImGui::CollapsingHeader(func.name.c_str())) {
              // ...
            }
          }
        }
        if (ImGui::CollapsingHeader("Basis functions")) {
          for (const auto &func : e_scene.bases) {
            if (ImGui::CollapsingHeader(func.name.c_str())) {
              // ...
            }
          }
        }
      }
      ImGui::End();
    }
  };
} // namespace met