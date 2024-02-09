#pragma once

#include <metameric/core/scene.hpp>
#include <metameric/core/scheduler.hpp>
#include <metameric/components/views/detail/imgui.hpp>

namespace met::detail {
  struct ImGuiEditInfo {
    // Push imgui components inside a TreeNode section,
    // or inline directly instead
    bool inside_tree   = true;

    // Allow adding of objects
    bool enable_addition = true;

    // Allow deletion of objects
    bool enable_delete = true;

    // Allow editing of object name
    bool enable_name_editing = true;

    // Allow editing of object data
    bool enable_value_editing = true;
  };

  template <typename Ty>
  using ImGuiComponentVisitor = std::function<void (SchedulerHandle &, Ty &)>;

  // Helpers for generating unified component editor layouts, with appropriate save state
  // handling and imgui state wrapping in place
  template <typename Ty>
  void push_component_edit(SchedulerHandle &info, uint i, ImGuiEditInfo edit_info, ImGuiComponentVisitor<Ty> visitor);
  template <typename Ty>
  void push_components_edit(const std::string &section_name, SchedulerHandle &info, ImGuiEditInfo edit_info, ImGuiComponentVisitor<Ty> visitor);

  // Defaults for component editor layout for most components
  void push_object_edit(SchedulerHandle &info, uint i, ImGuiEditInfo edit_info = { });
  void push_emitter_edit(SchedulerHandle &info, uint i, ImGuiEditInfo edit_info = { });
  void push_uplifting_edit(SchedulerHandle &info, uint i, ImGuiEditInfo edit_info = { });
  void push_colr_system_edit(SchedulerHandle &info, uint i, ImGuiEditInfo edit_info = { });
  void push_objects_edit(SchedulerHandle &info, ImGuiEditInfo edit_info = { });
  void push_emitters_edit(SchedulerHandle &info, ImGuiEditInfo edit_info = { });
  void push_upliftings_edit(SchedulerHandle &info, ImGuiEditInfo edit_info = { });
  void push_colr_systems_edit(SchedulerHandle &info, ImGuiEditInfo edit_info = { });
} // namespace met::detail