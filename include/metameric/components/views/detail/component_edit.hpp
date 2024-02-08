#pragma once

#include <metameric/core/scheduler.hpp>

namespace met::detail {
  struct ImGuiEditInfo {
    // Push imgui components inside a TreeNode section
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

  // Describe imgui layout for editing object i
  void push_imgui_object_edit(SchedulerHandle &info, uint i, ImGuiEditInfo edit_info = { });
  void push_imgui_objects_edit(SchedulerHandle &info, ImGuiEditInfo edit_info = { });

  // Describe imgui layout for editing emitter i
  void push_imgui_emitter_edit(SchedulerHandle &info, uint i, ImGuiEditInfo edit_info = { });

  // Describe imgui layout for editing uplifting i
  void push_imgui_uplifting_edit(SchedulerHandle &info, uint i, ImGuiEditInfo edit_info = { });

  // Describe imgui layout for editing color system i
  void push_imgui_csys_edit(SchedulerHandle &info, uint i, ImGuiEditInfo edit_info = { });
} // namespace met::detail