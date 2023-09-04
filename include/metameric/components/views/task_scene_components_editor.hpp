#pragma once

#include <metameric/core/scheduler.hpp>
#include <metameric/core/scene_handler.hpp>
#include <metameric/components/views/detail/imgui.hpp>
#include <format>

namespace met {
  namespace detail {
    constexpr
    auto fun_resource_selector = [](std::string_view title, const auto &resources, uint &j) {
      if (ImGui::BeginCombo(title.data(), resources[j].name.c_str())) {
        for (uint i = 0; i < resources.size(); ++i)
          if (ImGui::Selectable(resources[i].name.c_str(), j == i))
            j = i;
        ImGui::EndCombo();
      } // if (BeginCombo)
    };
  } // namespace detail

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

            // Add treenode section; postpone jumping into section
            bool open_section = ImGui::TreeNodeEx(component.name.c_str());
            
            // Insert delete button, is_active button on same
            ImGui::SameLine(ImGui::GetContentRegionMax().x - 38.f);
            if (ImGui::SmallButton(object.is_active ? "V" : "H"))
              object.is_active = !object.is_active;
            ImGui::SameLine(ImGui::GetContentRegionMax().x - 16.f);
            if (ImGui::SmallButton("X")) {
              info.global("scene_handler").writeable<SceneHandler>().touch({
                .name = "Delete object",
                .redo = [i = i]                         (auto &scene) { scene.components.objects.erase(i); },
                .undo = [o = e_scene.components.objects](auto &scene) { scene.components.objects = o;      }
              });
              break;
            }

            if (open_section) {
              // Object mesh/uplifting selection
              detail::fun_resource_selector("Uplifting", e_scene.components.upliftings, object.uplifting_i);
              detail::fun_resource_selector("Mesh", e_scene.resources.meshes, object.mesh_i);

              // Diffuse section
              ImGui::Separator();
              if (ImGui::BeginCombo("Diffuse data", object.diffuse.index() ? "Texture" : "Value")) {
                if (ImGui::Selectable("Value", object.diffuse.index() == 0))
                  object.diffuse.emplace<0>(Colr(1));
                if (ImGui::Selectable("Texture", object.diffuse.index() == 1))
                  object.diffuse.emplace<1>(0u);
                ImGui::EndCombo();
              } // If (BeginCombo)
              if (object.diffuse.index() == 0) {
                ImGui::ColorEdit3("Diffuse color", std::get<0>(object.diffuse).data());
              } else {
                detail::fun_resource_selector("Diffuse texture", e_scene.resources.images, std::get<1>(object.diffuse));
              }
              
              // Roughness section
              ImGui::Separator();
              if (ImGui::BeginCombo("Roughness data", object.roughness.index() ? "Texture" : "Value")) {
                if (ImGui::Selectable("Value", object.roughness.index() == 0))
                  object.roughness.emplace<0>(0.f);
                if (ImGui::Selectable("Texture", object.roughness.index() == 1))
                  object.roughness.emplace<1>(0u);
                ImGui::EndCombo();
              } // If (BeginCombo)
              if (object.roughness.index() == 0) {
                ImGui::InputFloat("Roughness value", &std::get<0>(object.roughness));
              } else {
                detail::fun_resource_selector("Roughness texture", e_scene.resources.images, std::get<1>(object.roughness));
              }
              
              // Metallic section
              ImGui::Separator();
              if (ImGui::BeginCombo("Metallic data", object.metallic.index() ? "Texture" : "Value")) {
                if (ImGui::Selectable("Value", object.metallic.index() == 0))
                  object.metallic.emplace<0>(0.f);
                if (ImGui::Selectable("Texture", object.metallic.index() == 1))
                  object.metallic.emplace<1>(0u);
                ImGui::EndCombo();
              } // If (BeginCombo)
              if (object.metallic.index() == 0) {
                ImGui::InputFloat("Metallic value", &std::get<0>(object.metallic));
              } else {
                detail::fun_resource_selector("Metallic texture", e_scene.resources.images, std::get<1>(object.metallic));
              }
              
              // Opacity section
              ImGui::Separator();
              if (ImGui::BeginCombo("Opacity data", object.opacity.index() ? "Texture" : "Value")) {
                if (ImGui::Selectable("Value", object.opacity.index() == 0))
                  object.opacity.emplace<0>(1.f);
                if (ImGui::Selectable("Texture", object.opacity.index() == 1))
                  object.opacity.emplace<1>(0u);
                ImGui::EndCombo();
              } // If (BeginCombo)
              if (object.opacity.index() == 0) {
                ImGui::InputFloat("Opacity value", &std::get<0>(object.opacity));
              } else {
                detail::fun_resource_selector("Opacity texture", e_scene.resources.images, std::get<1>(object.opacity));
              }
              
              ImGui::TreePop();
            } // if (open_section)

            // Handle modifications to object copy
            if (object != component.value) {
              info.global("scene_handler").writeable<SceneHandler>().touch({
                .name = "Modify object",
                .redo = [i = i, obj = object         ](auto &scene) { scene.components.objects[i].value = obj; },
                .undo = [i = i, obj = component.value](auto &scene) { scene.components.objects[i].value = obj; }
              });
            }

            ImGui::PopID();
          } // for (uint i)
          
          ImGui::PopID();
        } // if (collapsing header)

        if (ImGui::CollapsingHeader(std::format("Emitters ({})", e_scene.components.emitters.size()).c_str())) {
          ImGui::PushID("emitter_data");

          // Iterate over all objects
          for (uint i = 0; i < e_scene.components.emitters.size(); ++i) {
            guard_break(i < e_scene.components.emitters.size()); // Gracefully handle a deletion

            ImGui::PushID(std::format("emitter_data_{}", i).c_str());

            // We copy the emitter, and then test for changes
            const auto &component      = e_scene.components.emitters[i];
                  auto emitter         = component.value;

            // Add treenode section; postpone jumping into section
            bool open_section = ImGui::TreeNodeEx(component.name.c_str());
            
            // Insert delete button, is_active button on same
            ImGui::SameLine(ImGui::GetContentRegionMax().x - 38.f);
            if (ImGui::SmallButton(emitter.is_active ? "V" : "H"))
              emitter.is_active = !emitter.is_active;
            ImGui::SameLine(ImGui::GetContentRegionMax().x - 16.f);
            if (ImGui::SmallButton("X")) {
              info.global("scene_handler").writeable<SceneHandler>().touch({
                .name = "Delete emitter",
                .redo = [i = i]                         (auto &scene) { scene.components.emitters.erase(i); },
                .undo = [o = e_scene.components.emitters](auto &scene) { scene.components.emitters = o;      }
              });
              break;
            }

            if (open_section) {
              detail::fun_resource_selector("Illuminant", e_scene.resources.illuminants, emitter.illuminant_i);
              ImGui::InputFloat("Power multiplier", &emitter.multiplier);
              
              // ...
              
              ImGui::TreePop();
            } // if (open_section)

            // Handle modifications to emitter copy
            if (emitter != component.value) {
              info.global("scene_handler").writeable<SceneHandler>().touch({
                .name = "Modify emitter",
                .redo = [i = i, obj = emitter        ](auto &scene) { scene.components.emitters[i].value = obj; },
                .undo = [i = i, obj = component.value](auto &scene) { scene.components.emitters[i].value = obj; }
              });
            }

            ImGui::PopID();
          } // for (uint i)
          ImGui::PopID();
        } // if (collapsing header)

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