#pragma once

#include <metameric/core/scene.hpp>
#include <metameric/core/utility.hpp>
#include <metameric/core/scheduler.hpp>
#include <metameric/components/views/detail/imgui.hpp>

namespace met {
  namespace detail {
    // Info object for customizing behavior of push_*_editor() and related methods
    struct ImGuiEditInfo {
      std::string editor_name = "Editor"; // Surrounding editor section name
      bool inside_tree        = true;     // Push imgui components inside a TreeNode section,  or inline directly
      bool show_add           = true;     // Allow adding of components to lists
      bool show_del           = true;     // Allow deletion of components
      bool edit_name          = true;     // Allow editing of component name
      bool edit_data          = true;     // Allow editing of component data
    };

    // Visitor closure that allows editing of a component/resource inside push_*_editor(),
    // while the enclosing method takes care of state editing and undo/redo saving and stuff.
    template <typename Ty> requires (is_scene_data<Ty>)
    using ImGuiEditVisitor = std::conditional_t<is_component<Ty>,
                                                std::function<void (SchedulerHandle &,       Ty &)>,
                                                std::function<void (SchedulerHandle &, const Ty &)>>;

    // Default instantiations for ImGuiEditVisitor that encapsulate components/resources;
    // see component_edit.cpp for implementations
    template <typename Ty> requires(is_component<Ty>)
    void edit_visitor_default(SchedulerHandle &, Ty &);
    template <typename Ty> requires (is_resource<Ty>)
    void edit_visitor_default(SchedulerHandle &, const Ty &);
    template <> void edit_visitor_default(SchedulerHandle &, Component<Object> &);
    template <> void edit_visitor_default(SchedulerHandle &, Component<Emitter> &);
    template <> void edit_visitor_default(SchedulerHandle &, Component<Uplifting> &);
    template <> void edit_visitor_default(SchedulerHandle &, Component<ColorSystem> &);

    // Helper method; encapsulate a scene component whose data can be edited by a visitor closure, 
    // s.t. this surrounding method handles scene save state updating
    template <typename Ty>
    void encapsulate_component_data(SchedulerHandle     &info,
                                    uint                 component_i,
                                    ImGuiEditVisitor<Ty> visitor) {
      met_trace();
      
      // We copy the component's value for modification
      const auto &e_scene   = info.global("scene").getr<Scene>();
      const auto &component = e_scene.components_by_type<Ty>()[component_i];
            auto copy       = component;
      
      // Visitor closure potentially modifies value; return if no modifications were made
      visitor(info, copy);
      guard(copy != component);

      // Submit scene edit s.t. redo/undo modifications are recorded
      info.global("scene").getw<Scene>().touch({
        .name = "Modify component data",
        .redo = [component_i, component = copy](auto &scene)       { 
          scene.components_by_type<Ty>()[component_i] = component; },
        .undo = [component_i, component = component](auto &scene)  { 
          scene.components_by_type<Ty>()[component_i] = component; }
      });
    }

    // Helper method; encapsulate a scene component whose name be edited by a visitor closure,
    // s.t. this surrounding method handles scene save state updating
    template <typename Ty> requires (is_scene_data<Ty>)
    void encapsulate_component_name(SchedulerHandle &info,
                                    uint component_i,
                                    std::function<bool (SchedulerHandle &, std::string&)> visitor) {
      met_trace();

      constexpr static auto str_edit_flags = ImGuiInputTextFlags_AutoSelectAll 
                                          | ImGuiInputTextFlags_EnterReturnsTrue;

      // We copy the component's name for modification
      const auto &e_scene   = info.global("scene").getr<Scene>();
      const auto &component = e_scene.components_by_type<Ty>()[component_i];
            auto copy       = component.name;

      // Visitor closure potentially modifies name; return if no modifications were made
      guard(visitor(info, copy));
      guard(copy != component.name);

      // Submit scene edit s.t. redo/undo modifications are recorded
      info.global("scene").getw<Scene>().touch({
        .name = "Modify component name",
        .redo = [component_i, name = copy](auto &scene)           {
          scene.components_by_type<Ty>()[component_i].name = name;  },
        .undo = [component_i, name = component.name](auto &scene) {
          scene.components_by_type<Ty>()[component_i].name = name;  }
      });
    }
  } // namespace detail

  // Helper method;  spawn a sensible editor view with name editing, activity flags, a delete
  // button, and a default visitor for editing a scene component/resource's contained data
  template <typename Ty> requires (is_scene_data<Ty>)
  void push_editor(SchedulerHandle             &info, 
                   uint                         component_i,
                   detail::ImGuiEditInfo        edit_info = {},
                   detail::ImGuiEditVisitor<Ty> visitor   = detail::edit_visitor_default<Ty>) {
    met_trace();

    // Set local scope ID j.i.c.
    auto _scope = ImGui::ScopedID(std::format("{}_edit_{}", typeid(Ty).name(), component_i));

    // Get external resources and shorthands
    const auto &e_scene   = info.global("scene").getr<Scene>();
    const auto &component = e_scene.components_by_type<Ty>()[component_i];

    // If requested, spawn a TreeNode.
    bool section_open = !edit_info.inside_tree || ImGui::TreeNodeEx(component.name.c_str());

    // Is_active button, on same line as tree node if available
    if constexpr (has_active_value<typename Ty::value_type>) {
      if (edit_info.inside_tree && edit_info.edit_data) {
        ImGui::SameLine(ImGui::GetContentRegionMax().x - 38.f);
        encapsulate_component_data<Ty>(info, component_i, [](auto &info, auto &component) {
          if (ImGui::SmallButton(component.value.is_active ? "V" : "H"))
            component.value.is_active = !component.value.is_active;
        });
      } // if (inside_tree && edit_data)
    } // if (has_active_value)

    // Delete button, on same line as tree node if available
    if (edit_info.inside_tree && edit_info.show_del) {
      ImGui::SameLine(ImGui::GetContentRegionMax().x - 16.f);
      if (ImGui::SmallButton("X")) {
        info.global("scene").getw<Scene>().touch({
          .name = "Delete component",
          .redo = [component_i] (auto &scene)                                { 
            scene.components_by_type<Ty>().erase(component_i);               },
          .undo = [component_i, component](auto &scene)                      { 
            scene.components_by_type<Ty>().insert(component_i, component); }
        });
        return;
      }
    } // if (inside_tree && show_del)

    // If the section is closed, we can now return early
    guard(section_open);
    
    // Optionally spawn component name editor
    if (edit_info.edit_name) {
      encapsulate_component_name<Ty>(info, component_i, [](auto &info, auto &name) {
        constexpr static auto str_edit_flags = ImGuiInputTextFlags_AutoSelectAll 
                                            | ImGuiInputTextFlags_EnterReturnsTrue;
        return ImGui::InputText("Name", &name, str_edit_flags);
      });
    }
    
    // Encapsulate component visitor, to safely run value editing code
    if (edit_info.edit_data)
      encapsulate_component_data<Ty>(info, component_i, visitor);
    
    // If the section needs closing, do so last
    if (edit_info.inside_tree)
      ImGui::TreePop();
  }

  // Helper method;  spawn a sensible editor view with name editing, activity flags, a delete
  // button, and a default visitor for editing the data of a whole group of scene components/resources
  template <typename Ty> requires (is_scene_data<Ty>)
  void push_editor(SchedulerHandle             &info, 
                   detail::ImGuiEditInfo        edit_info = {},
                   detail::ImGuiEditVisitor<Ty> visitor   = detail::edit_visitor_default<Ty>) {
    met_trace();

    // Set local scope ID j.i.c.
    auto _scope = ImGui::ScopedID(std::format("{}_list", typeid(Ty).name()));
    
    // Get external resources and shorthands
    const auto &e_scene      = info.global("scene").getr<Scene>();
    const auto &e_components = e_scene.components_by_type<Ty>();
    
    // If requested, spawn a TreeNode
    bool section_open = !edit_info.inside_tree || ImGui::CollapsingHeader(edit_info.editor_name.c_str(), ImGuiTreeNodeFlags_DefaultOpen);

    // If the section is closed, we can now return early
    guard(section_open);

    // Iterate over all relevant components
    for (uint i = 0; i < e_components.size(); ++i) {
      guard_break(i < e_components.size()); // Gracefully handle a deletion

      // If inlining, add a visual separator between each component
      if (i > 0 && !edit_info.inside_tree)
        ImGui::Separator();
      
      // Visit component internals
      push_editor<Ty>(info, i, edit_info, visitor);
    } // for (uint i)

    // Handle creation of new components
    if (edit_info.show_add) {
      // If there are listed components, visually separate this section
      if (!e_components.empty())
        ImGui::Separator();
      
      // Spawn an addition button to the right
      ImGui::NewLine();
      ImGui::SameLine(ImGui::GetContentRegionMax().x - 32.f);
      if (ImGui::SmallButton("Add")) {
        info.global("scene").getw<Scene>().touch({
          .name = "Add component",
          .redo = [](auto &scene) {
            scene.components_by_type<Ty>().push("New component", { }); },
          .undo = [](auto &scene) {
            scene.components_by_type<Ty>().erase(scene.components_by_type<Ty>().size() - 1); }
        });
      }
    }
  }
} // namespace met