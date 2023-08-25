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
        
      }
      ImGui::End();
    }
  };
} // namespace met