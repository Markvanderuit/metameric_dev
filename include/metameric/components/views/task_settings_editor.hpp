#pragma once

#include <metameric/core/scene_handler.hpp>
#include <metameric/core/scheduler.hpp>
#include <metameric/core/settings.hpp>
#include <metameric/core/utility.hpp>
#include <metameric/components/views/detail/imgui.hpp>

namespace met {
  struct SettingsEditorTask : public detail::TaskNode {
    void eval(SchedulerHandle &info) override {
      met_trace();

      // Track killing of own task
      bool is_settings_open = true;
      
      if (ImGui::Begin("Settings", &is_settings_open, ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoDocking)) {
        // Get external resources
        const auto &e_scene_handler       = info.global("scene_handler").read_only<SceneHandler>();
        const auto &[e_settings, e_state] = e_scene_handler.scene.settings;

        // Texture name helper
        std::array<std::string, 4> texture_names = { "Full", "High", "Medium", "Low" };
        uint texture_i = static_cast<uint>(e_settings.texture_size);

        // Copy of settings to detect modification
        auto settings = e_settings;

        // Combobox to selext texture size setting
        if (ImGui::BeginCombo("Texture size", texture_names[texture_i].c_str())) {
          for (uint i = 0; i < texture_names.size(); ++i) {
            if (ImGui::Selectable(texture_names[i].c_str())/* , i == texture_i */) {
              settings.texture_size = static_cast<Settings::TextureSize>(i);
              fmt::print("Clicked {}\n", i);
            }
          }
          ImGui::EndCombo();
        }

        // Test if settings are changed, and apply
        if (settings != e_settings) {
          auto &e_scene_handler = info.global("scene_handler").writeable<SceneHandler>();
          e_scene_handler.scene.settings.value = settings;
          fmt::print("Apply\n");
        }
      }
      ImGui::End();

      // Kill own task
      if (!is_settings_open)
        info.task().dstr();
    }
  };
} // namespace met