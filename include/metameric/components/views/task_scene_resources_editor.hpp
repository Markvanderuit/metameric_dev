#pragma once

#include <metameric/core/scheduler.hpp>
#include <metameric/core/scene_handler.hpp>
#include <metameric/components/views/detail/imgui.hpp>
#include <metameric/components/views/detail/file_dialog.hpp>
#include <format>

namespace met {
  class SceneResourcesEditorTask : public detail::TaskNode {
    
  public:
    void eval(SchedulerHandle &info) override {
      met_trace_full();

      ImGui::ShowDemoWindow();

      if (ImGui::Begin("Scene resources", nullptr, ImGuiWindowFlags_MenuBar)) {
        // Get external resources
        const auto &e_handler = info.global("scene_handler").read_only<SceneHandler>();
        const auto &e_scene   = e_handler.scene;

        if (ImGui::BeginMenuBar()) {
          if (ImGui::BeginMenu("Import")) {
            if (ImGui::MenuItem("Wavefront (.obj)")) {
              if (fs::path path; detail::load_dialog(path, "obj")) {
                auto &e_handler = info.global("scene_handler").writeable<SceneHandler>();
                e_handler.import_wavefront_obj(path);
              }
            }

            if (ImGui::MenuItem("Texture (RGB) (.exr, .png, .jpg, ...)")) {
              
            }

            if (ImGui::MenuItem("Texture (A) (.exr, .png, .jpg, ...)")) {
              
            }

            if (ImGui::MenuItem("Spectral functions")) {
              
            }

            if (ImGui::MenuItem("Observer functions")) {
              
            }

            if (ImGui::MenuItem("Basis functions")) {
              
            }
            
            ImGui::EndMenu();
          }          

          ImGui::EndMenuBar();
        }

        if (ImGui::CollapsingHeader(std::format("Meshes ({})", e_scene.meshes.size()).c_str())) {
          for (const auto &mesh : e_scene.meshes) {
            if (ImGui::TreeNodeEx(mesh.name.c_str(), ImGuiTreeNodeFlags_Leaf)) {
              if (ImGui::IsItemHovered()) {
                ImGui::BeginTooltip();
                ImGui::Value("Vertices", static_cast<uint>(mesh.value().verts.size()));
                ImGui::Value("Elements", static_cast<uint>(mesh.value().elems.size()));
                ImGui::EndTooltip();
              }

              ImGui::SameLine(ImGui::GetContentRegionMax().x - 16.f);

              if (ImGui::SmallButton("X")) {
                debug::check_expr(false, "Not implemented");
              } 
              
              ImGui::TreePop();
            }
          }
        }

        if (ImGui::CollapsingHeader(std::format("Textures ({})", e_scene.textures_3f.size() + 
                                                                 e_scene.textures_1f.size()).c_str())) {
          for (const auto &txtr : e_scene.textures_3f) {
            if (ImGui::TreeNodeEx(txtr.name.c_str(), ImGuiTreeNodeFlags_Leaf)) {
              if (ImGui::IsItemHovered()) {
                ImGui::BeginTooltip();
                ImGui::Text("Dimensions: %i x %i", txtr.value().size()[0], txtr.value().size()[1]);
                ImGui::Value("Channels", 3);
                ImGui::EndTooltip();
              }

              ImGui::SameLine(ImGui::GetContentRegionMax().x - 16.f);

              if (ImGui::SmallButton("X")) {
                debug::check_expr(false, "Not implemented");
              } 
              
              ImGui::TreePop();
            }
          }
          for (const auto &txtr : e_scene.textures_1f) {
            if (ImGui::TreeNodeEx(txtr.name.c_str(), ImGuiTreeNodeFlags_Leaf)) {
              if (ImGui::IsItemHovered()) {
                ImGui::BeginTooltip();
                ImGui::Text("Dimensions: %i x %i", txtr.value().size()[0], txtr.value().size()[1]);
                ImGui::Value("Channels", 1);
                ImGui::EndTooltip();
              }

              ImGui::SameLine(ImGui::GetContentRegionMax().x - 16.f);

              if (ImGui::SmallButton("X")) {
                debug::check_expr(false, "Not implemented");
              } 
              
              ImGui::TreePop();
            }
          }
        }

        if (ImGui::CollapsingHeader(std::format("Illuminant functions ({})", e_scene.illuminants.size()).c_str())) {
          for (const auto &func : e_scene.illuminants) {
            if (ImGui::TreeNodeEx(func.name.c_str(), ImGuiTreeNodeFlags_Leaf)) {
              ImGui::SameLine(ImGui::GetContentRegionMax().x - 16.f);

              if (ImGui::SmallButton("X")) {
                debug::check_expr(false, "Not implemented");
              } 
              
              ImGui::TreePop();
            }
          }
        }

        if (ImGui::CollapsingHeader(std::format("Observer functions ({})", e_scene.observers.size()).c_str())) {
          for (const auto &func : e_scene.observers) {
            if (ImGui::CollapsingHeader(func.name.c_str())) {
              // ...
            }
          }
        }

        if (ImGui::CollapsingHeader(std::format("Basis functions ({})", e_scene.bases.size()).c_str())) {
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