#pragma once

#include <metameric/core/scene.hpp>
#include <metameric/core/scheduler.hpp>
#include <metameric/components/views/detail/imgui.hpp>

namespace met::detail {
  struct ImGuiEditInfo {
    bool inside_tree          = true; // Push imgui components inside a TreeNode section,  or inline directly
    bool enable_addition      = true; // Allow adding of components to lists
    bool enable_delete        = true; // Allow deletion of components
    bool enable_name_editing  = true; // Allow editing of components name
    bool enable_value_editing = true; // Allow editing of components data
  };

  template <typename Ty>
  using ImGuiComponentVisitor = std::function<void (SchedulerHandle &, Component<Ty> &)>;

  // Helpers for generating unified component editor layouts, with appropriate save state
  // handling and imgui state to encapsulate it
  template <typename Ty>
  void push_component_edit(SchedulerHandle &info, uint i, ImGuiEditInfo edit_info, ImGuiComponentVisitor<Ty> visitor);
  template <typename Ty>
  void push_components_edit(const std::string &section_name, SchedulerHandle &info, ImGuiEditInfo edit_info, ImGuiComponentVisitor<Ty> visitor);

  // Sensible defaults for component editor layout for most scene components
  void push_object_edit(SchedulerHandle &info, uint i, ImGuiEditInfo edit_info = { });
  void push_emitter_edit(SchedulerHandle &info, uint i, ImGuiEditInfo edit_info = { });
  void push_uplifting_edit(SchedulerHandle &info, uint i, ImGuiEditInfo edit_info = { });
  void push_colr_system_edit(SchedulerHandle &info, uint i, ImGuiEditInfo edit_info = { });
  void push_objects_edit(SchedulerHandle &info, ImGuiEditInfo edit_info = { });
  void push_emitters_edit(SchedulerHandle &info, ImGuiEditInfo edit_info = { });
  void push_upliftings_edit(SchedulerHandle &info, ImGuiEditInfo edit_info = { });
  void push_colr_systems_edit(SchedulerHandle &info, ImGuiEditInfo edit_info = { });
} // namespace met::detail