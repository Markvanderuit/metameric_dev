#pragma once

#include <metameric/core/scheduler.hpp>
#include <metameric/core/scene_handler.hpp>
#include <metameric/components/views/detail/imgui.hpp>
#include <format>

namespace met {
  class SceneComponentsEditorTask : public detail::TaskNode {
    
  public:
    void eval(SchedulerHandle &info) override {
      met_trace_full();

      if (ImGui::Begin("Scene components")) {
        // Get external resources
        const auto &e_handler = info.global("scene_handler").read_only<SceneHandler>();
        const auto &e_scene   = e_handler.scene;
        
        if (ImGui::CollapsingHeader(std::format("Objects ({})", e_scene.components.objects.size()).c_str())) {
          ImGui::PushID("object_data");

          // Iterate over all objects
          for (uint i = 0; i < e_scene.components.objects.size(); ++i) {
            guard_break(i < e_scene.components.objects.size()); // Gracefully handle a deletion
            
            ImGui::PushID(std::format("object_data_{}", i).c_str());
            
            // We copy the object, and then test for changes
            const auto &component      = e_scene.components.objects[i];
                  auto object          = component.value;
            const auto &mesh_component = e_scene.resources.meshes[object.mesh_i];

            // Collapsible section to modify object
            if (ImGui::TreeNodeEx(component.name.c_str())) {
              if (ImGui::BeginCombo("Mesh", mesh_component.name.c_str())) {
                for (uint j = 0; j < e_scene.resources.meshes.size(); ++j) {
                  if (ImGui::Selectable(e_scene.resources.meshes[j].name.c_str(), j == object.mesh_i)) {
                    object.mesh_i = j;
                  }
                } // for (uint j)
                ImGui::EndCombo();
              }

              ImGui::TreePop();
            }

            // Handle modifications to object copy
            if (object != component.value)
              info.global("scene_handler").writeable<SceneHandler>().touch({
                .name = "Modify object",
                .redo = [i = i, obj = object         ](auto &scene) { scene.components.objects[i].value = obj; },
                .undo = [i = i, obj = component.value](auto &scene) { scene.components.objects[i].value = obj; }
              });

            // Delete button at end of line
            ImGui::SameLine(ImGui::GetContentRegionMax().x - 16.f);
            if (ImGui::SmallButton("X")) {
              info.global("scene_handler").writeable<SceneHandler>().touch({
                .name = "Delete object",
                .redo = [i = i]                         (auto &scene) { scene.components.objects.erase(i); },
                .undo = [o = e_scene.components.objects](auto &scene) { scene.components.objects = o;      }
              });
              break;
            }

            ImGui::PopID();
          } // for (uint i)
          
          ImGui::PopID();
        } // if (collapsing header)

        if (ImGui::CollapsingHeader(std::format("Materials ({})", e_scene.components.materials.size()).c_str())) {
          for (const auto &object : e_scene.components.materials) {
            if (ImGui::TreeNodeEx(object.name.c_str(), ImGuiTreeNodeFlags_Leaf)) {
              ImGui::SameLine(ImGui::GetContentRegionMax().x - 16.f);

              if (ImGui::SmallButton("X")) {
                debug::check_expr(false, "Not implemented");
              } 
              
              ImGui::TreePop();
            }
          }
        }

        if (ImGui::CollapsingHeader(std::format("Upliftings ({})", e_scene.components.upliftings.size()).c_str())) {
          for (const auto &object : e_scene.components.upliftings) {
            if (ImGui::TreeNodeEx(object.name.c_str(), ImGuiTreeNodeFlags_Leaf)) {
              ImGui::SameLine(ImGui::GetContentRegionMax().x - 16.f);

              if (ImGui::SmallButton("X")) {
                debug::check_expr(false, "Not implemented");
              } 
              
              ImGui::TreePop();
            }
          }
        }

        if (ImGui::CollapsingHeader(std::format("Emitters ({})", e_scene.components.emitters.size()).c_str())) {
          for (const auto &object : e_scene.components.emitters) {
            if (ImGui::TreeNodeEx(object.name.c_str(), ImGuiTreeNodeFlags_Leaf)) {
              ImGui::SameLine(ImGui::GetContentRegionMax().x - 16.f);

              if (ImGui::SmallButton("X")) {
                debug::check_expr(false, "Not implemented");
              } 
              
              ImGui::TreePop();
            }
          }
        }


        if (ImGui::CollapsingHeader(std::format("Color systems ({})", e_scene.components.colr_systems.size()).c_str())) {
          for (const auto &object : e_scene.components.colr_systems) {
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