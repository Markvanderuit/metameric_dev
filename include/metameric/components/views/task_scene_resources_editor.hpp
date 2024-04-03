#pragma once

#include <metameric/core/scheduler.hpp>
#include <metameric/core/scene.hpp>
#include <metameric/components/views/detail/imgui.hpp>
#include <metameric/components/views/detail/file_dialog.hpp>
#include <format>

namespace met {
  class SceneResourcesEditorTask : public detail::TaskNode {
  public:
    void eval(SchedulerHandle &info) override {
      met_trace_full();

      // Get external resources
      const auto &e_scene = info.global("scene").getr<Scene>();

      // Spawn view of texture atlas interiors
      if (ImGui::Begin("Texture atlas")) {
        const auto &e_atlas = e_scene.resources.images.gl.texture_atlas_3f;
        // Spawn views
        for (uint i = 0; i < e_atlas.capacity().z(); ++i) {
          for (uint j = 0; j < e_atlas.levels(); ++j) {
            ImGui::Image(ImGui::to_ptr(e_atlas.view(i, j).object()), { 256, 256 });
            if (j < e_atlas.levels() - 1)
              ImGui::SameLine();
          }
        }
      }
      ImGui::End();

      // Spawn view of weight atlas interiors
      if (ImGui::Begin("Bary atlas")) {
        const auto &e_bary_data = e_scene.components.upliftings.gl;
        for (uint i = 0; i < e_bary_data.texture_barycentrics.capacity().z(); ++i) {
          const auto &view = e_bary_data.texture_barycentrics.view(i, 0);
          ImGui::Image(ImGui::to_ptr(view.object()), { 1024, 1024 }, { 0, 0 }, { 1, 1 }, ImVec4(1, 1, 1, 1), ImVec4(1, 1, 1, 1));
        }
      }
      ImGui::End();

      if (ImGui::Begin("Scene resources")) {
        // Get external resources
        const auto &e_scene = info.global("scene").getr<Scene>();

        if (ImGui::CollapsingHeader(std::format("Meshes ({})", e_scene.resources.meshes.size()).c_str())) {
          for (const auto &mesh : e_scene.resources.meshes) {
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

        if (ImGui::CollapsingHeader(std::format("Images ({})", e_scene.resources.images.size()).c_str())) {
          for (uint i = 0; i < e_scene.resources.images.size(); ++i) {
            const auto &image = e_scene.resources.images[i];
            if (ImGui::TreeNodeEx(image.name.c_str(), ImGuiTreeNodeFlags_Leaf)) {
              if (ImGui::IsItemHovered()) {
                ImGui::BeginTooltip();

                ImGui::Text("Dimensions: %i x %i", image.value().size()[0], image.value().size()[1]);
                ImGui::Value("Channels", image.value().channels());
                
                /* if (e_txtr_data.info.size() > i) {
                  const auto &info = e_txtr_data.info[i];
                  const auto *txtr = info.is_3f ? (gl::AbstractTexture*) &e_txtr_data.views_3f[info.layer]
                                                : (gl::AbstractTexture*) &e_txtr_data.views_1f[info.layer];
                  ImGui::Image(ImGui::to_ptr(txtr->object()), { 128, 128 }, info.uv0, (info.uv0 + info.uv1).eval());
                } */

                ImGui::EndTooltip();
              }

              ImGui::SameLine(ImGui::GetContentRegionMax().x - 16.f);

              if (ImGui::SmallButton("X")) {
                debug::check_expr(false, "Not implemented");
              } 
              
              ImGui::TreePop();
            } // if (treenode)
          } // for (i)
        } // if (header)

        if (ImGui::CollapsingHeader(std::format("Illuminant functions ({})", e_scene.resources.illuminants.size()).c_str())) {
          for (const auto &func : e_scene.resources.illuminants) {
            if (ImGui::TreeNodeEx(func.name.c_str(), ImGuiTreeNodeFlags_Leaf)) {
              ImGui::SameLine(ImGui::GetContentRegionMax().x - 16.f);

              if (ImGui::SmallButton("X")) {
                debug::check_expr(false, "Not implemented");
              } 
              
              ImGui::TreePop();
            }
          }
        }

        if (ImGui::CollapsingHeader(std::format("Observer functions ({})", e_scene.resources.observers.size()).c_str())) {
          for (const auto &func : e_scene.resources.observers) {
            if (ImGui::CollapsingHeader(func.name.c_str())) {
              // ...
            }
          }
        }

        if (ImGui::CollapsingHeader(std::format("Basis functions ({})", e_scene.resources.bases.size()).c_str())) {
          for (const auto &func : e_scene.resources.bases) {
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