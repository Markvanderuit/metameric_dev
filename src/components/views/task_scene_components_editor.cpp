#include <metameric/core/scene.hpp>
#include <metameric/components/views/task_scene_components_editor.hpp>
#include <metameric/components/views/detail/imgui.hpp>
#include <metameric/components/views/detail/component_edit.hpp>
#include <format>

namespace met {
  void SceneComponentsEditorTask::eval(SchedulerHandle &info) {
    met_trace();
    if (ImGui::Begin("Scene components")) {
      push_objects_edit(info);
      push_emitters_edit(info);
      push_upliftings_edit(info);
      push_colr_systems_edit(info);
    }
    ImGui::End();
  }
} // namespace met