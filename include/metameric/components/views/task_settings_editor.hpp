#pragma once

#include <metameric/core/scene.hpp>
#include <metameric/core/scheduler.hpp>
#include <metameric/core/utility.hpp>
#include <metameric/components/views/detail/imgui.hpp>
#include <metameric/components/views/detail/component_edit.hpp>

namespace met {
  struct SettingsEditorTask : public detail::TaskNode {
    void eval(SchedulerHandle &info) override {
      met_trace();

      // Track killing of own task
      bool is_settings_open = true;
      
      if (ImGui::Begin("Settings", &is_settings_open, ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoDocking)) {
        // Get external resources
        const auto &e_scene               = info.global("scene").getr<Scene>();
        const auto &[e_settings, e_state] = e_scene.components.settings;

        // Texture name helper
        std::array<std::string, 4> texture_names = { "Full", "High", "Medium", "Low" };
        uint texture_i = static_cast<uint>(e_settings.texture_size);

        // Copy of settings to detect modification
        auto settings = e_settings;

        // Combobox to selext texture size setting
        if (ImGui::BeginCombo("Texture size", texture_names[texture_i].c_str())) {
          for (uint i = 0; i < texture_names.size(); ++i) {
            if (ImGui::Selectable(texture_names[i].c_str(), texture_i == i)) {
              texture_i = i;
            }
          }
          ImGui::EndCombo();
        }
        settings.texture_size = static_cast<Settings::TextureSize>(texture_i);

        // Selector for active view in scene viewport
        detail::push_resource_selector("Viewport", e_scene.components.views, settings.view_i);

        ImGui::DragFloat("Render scale", &settings.view_scale, .05f, .05f, 1.f);


        // Test if settings are changed, and apply
        if (settings != e_settings) {
          auto &e_scene = info.global("scene").getw<Scene>();
          e_scene.components.settings.value = settings;
        }
      }
      ImGui::End();

      // Kill own task
      if (!is_settings_open)
        info.task().dstr();
    }
  };
} // namespace met