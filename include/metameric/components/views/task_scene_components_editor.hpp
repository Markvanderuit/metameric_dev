#pragma once

#include <metameric/core/scheduler.hpp>
#include <metameric/core/scene_handler.hpp>
#include <metameric/components/views/detail/imgui.hpp>

namespace met {
  class SceneComponentsEditorTask : public detail::TaskNode {
    
  public:
    void eval(SchedulerHandle &info) override {
      met_trace_full();

      if (ImGui::Begin("Scene components")) {
        // Get external resources
        const auto &e_handler = info.global("scene_handler").read_only<SceneHandler>();
        const auto &e_scene   = e_handler.scene;
        
        if (ImGui::CollapsingHeader(std::format("Objects ({})", e_scene.objects.size()).c_str())) {
          for (const auto &object : e_scene.objects) {
            if (ImGui::TreeNodeEx(object.name.c_str(), ImGuiTreeNodeFlags_Leaf)) {
              ImGui::SameLine(ImGui::GetContentRegionMax().x - 16.f);

              if (ImGui::SmallButton("X")) {
                debug::check_expr(false, "Not implemented");
              } 
              
              ImGui::TreePop();
            }
          }
        }

        if (ImGui::CollapsingHeader(std::format("Materials ({})", e_scene.materials.size()).c_str())) {
          for (const auto &object : e_scene.materials) {
            if (ImGui::TreeNodeEx(object.name.c_str(), ImGuiTreeNodeFlags_Leaf)) {
              ImGui::SameLine(ImGui::GetContentRegionMax().x - 16.f);

              if (ImGui::SmallButton("X")) {
                debug::check_expr(false, "Not implemented");
              } 
              
              ImGui::TreePop();
            }
          }
        }

        if (ImGui::CollapsingHeader(std::format("Upliftings ({})", e_scene.upliftings.size()).c_str())) {
          for (const auto &object : e_scene.upliftings) {
            if (ImGui::TreeNodeEx(object.name.c_str(), ImGuiTreeNodeFlags_Leaf)) {
              ImGui::SameLine(ImGui::GetContentRegionMax().x - 16.f);

              if (ImGui::SmallButton("X")) {
                debug::check_expr(false, "Not implemented");
              } 
              
              ImGui::TreePop();
            }
          }
        }

        if (ImGui::CollapsingHeader(std::format("Emitters ({})", e_scene.emitters.size()).c_str())) {
          for (const auto &object : e_scene.emitters) {
            if (ImGui::TreeNodeEx(object.name.c_str(), ImGuiTreeNodeFlags_Leaf)) {
              ImGui::SameLine(ImGui::GetContentRegionMax().x - 16.f);

              if (ImGui::SmallButton("X")) {
                debug::check_expr(false, "Not implemented");
              } 
              
              ImGui::TreePop();
            }
          }
        }


        if (ImGui::CollapsingHeader(std::format("Color systems ({})", e_scene.colr_systems.size()).c_str())) {
          for (const auto &object : e_scene.colr_systems) {
            if (ImGui::TreeNodeEx(object.name.c_str(), ImGuiTreeNodeFlags_Leaf)) {
              ImGui::SameLine(ImGui::GetContentRegionMax().x - 16.f);

              if (ImGui::SmallButton("X")) {
                debug::check_expr(false, "Not implemented");
              } 
              
              ImGui::TreePop();
            }
          }
        }

      }
      ImGui::End();
    }
  };
} // namespace met