#pragma once

#include <metameric/core/ranges.hpp>
#include <metameric/scene/scene.hpp>
#include <metameric/core/utility.hpp>
#include <metameric/core/scheduler.hpp>
#include <metameric/components/views/detail/imgui.hpp>

namespace met {
  namespace detail {  
    // Concept; some components have active flags to enable/disable them in the scene
    template <typename Ty>
    concept has_active_value = requires (Ty ty) { { ty.is_active } -> std::same_as<bool&>; };
    
    // Info object for customizing behavior of push_*_editor() and related methods
    struct ImGuiEditInfo {
      std::string editor_name = "Editor"; // Surrounding editor section name
      bool inside_tree        = true;     // Push imgui components inside a TreeNode section,  or inline directly
      bool default_open       = false;    // Are component/resource editors expanded by default?
      bool show_add           = true;     // Allow adding of components to lists
      bool show_del           = true;     // Allow deletion of component/resource
      bool show_dupl          = true;     // Allow duplication of component/resource
      bool edit_name          = true;     // Allow editing of component/resource name
      bool edit_data          = true;     // Allow editing of component/resource data
    };

    // Visitor closure that allows editing of a component/resource inside push_*_editor(),
    // while the enclosing method takes care of state editing and undo/redo saving and stuff.
    template <typename Ty> requires (is_scene_data<Ty>)
    using ImGuiEditVisitor = std::conditional_t<is_component<Ty>,
                                                std::function<void (SchedulerHandle &, uint,       Ty &)>,
                                                std::function<void (SchedulerHandle &, uint, const Ty &)>>;

    // Default instantiations for ImGuiEditVisitor that encapsulate components/resources;
    // see component_edit.cpp for implementations
    template <typename Ty> requires(is_component<Ty>)
    void edit_visitor_default(SchedulerHandle &, uint i, Ty &);
    template <typename Ty> requires (is_resource<Ty>)
    void edit_visitor_default(SchedulerHandle &, uint i, const Ty &);
    template <> void edit_visitor_default(SchedulerHandle &, uint i, Component<Object> &);
    template <> void edit_visitor_default(SchedulerHandle &, uint i, Component<Emitter> &);
    template <> void edit_visitor_default(SchedulerHandle &, uint i, Component<Uplifting> &);
    template <> void edit_visitor_default(SchedulerHandle &, uint i, Component<View> &);
    template <> void edit_visitor_default(SchedulerHandle &, uint i, const Resource<Mesh>  &);
    template <> void edit_visitor_default(SchedulerHandle &, uint i, const Resource<Image> &);

    // Helper method; encapsulate a scene component whose data can be edited by a visitor closure, 
    // s.t. this surrounding method handles scene save state updating
    template <typename Ty> requires (is_scene_data<Ty>)
    void encapsulate_scene_data(SchedulerHandle     &info,
                                uint                 data_i,
                                ImGuiEditVisitor<Ty> visitor) {
      met_trace();
      
      // Get external resources and shorthands
      const auto &scene = info.global("scene").getr<Scene>();
      const auto &data  = scene_data_by_type<Ty>(scene)[data_i];

      // Visitor is able to or unable to edit data, dependent
      // on whether component/resource supports std::equality_comparable
      if constexpr (std::equality_comparable<Ty>) {
        // We copy the component's value for modification; visitor
        //  potentially modifies value; we return if no modifications were made
        auto copy  = data;
        visitor(info, data_i, copy);
        guard(copy != data);
        
        // Submit scene edit s.t. redo/undo modifications are recorded
        info.global("scene").getw<Scene>().touch({
          .name = "Modify component data",
          .redo = [data_i, data = copy](auto &scene)      { 
            scene_data_by_type<Ty>(scene)[data_i] = data; },
          .undo = [data_i, data = data](auto &scene)      { 
            scene_data_by_type<Ty>(scene)[data_i] = data; }
        });
      } else {
        // If component/resource is not comparable, visitor simply views data
        visitor(info, data_i, data);
      }
    }

    // Helper method; encapsulate a scene component whose name be edited by a visitor closure,
    // s.t. this surrounding method handles scene save state updating;
    // visitor returns bool on edit, to avoid single-character saves
    template <typename Ty> requires (is_scene_data<Ty>)
    void encapsulate_scene_name(SchedulerHandle &info,
                                uint             data_i,
                                std::function<bool (SchedulerHandle &, std::string&)> visitor) {
      met_trace();

      constexpr static auto str_edit_flags = ImGuiInputTextFlags_AutoSelectAll 
                                           | ImGuiInputTextFlags_EnterReturnsTrue;

      // We copy the component's name for modification
      const auto &scene = info.global("scene").getr<Scene>();
      const auto &data  = scene_data_by_type<Ty>(scene)[data_i];
            auto  copy  = data.name;

      // Visitor closure potentially modifies name; return if no modifications were made
      guard(visitor(info, copy));
      guard(copy != data.name);

      // Submit scene edit s.t. redo/undo modifications are recorded
      info.global("scene").getw<Scene>().touch({
        .name = "Modify component name",
        .redo = [data_i, name = copy](auto &scene)           {
          scene_data_by_type<Ty>(scene)[data_i].name = name; },
        .undo = [data_i, name = data.name](auto &scene)      {
          scene_data_by_type<Ty>(scene)[data_i].name = name; }
      });
    }

    // Helper function; given a title, access to a set of scene resources,
    // and a modifiable index pointing to one of those resources, spawn
    // a combo box for selecting said resource
    constexpr
    void push_resource_selector(std::string_view title, const auto &resources, uint &j) {
      if (ImGui::BeginCombo(title.data(), resources[j].name.c_str())) {
        for (uint i = 0; i < resources.size(); ++i)
          if (ImGui::Selectable(resources[i].name.c_str(), j == i))
            j = i;
        ImGui::EndCombo();
      } // if (BeginCombo)
    };

    // Helper function; given a title, access to a set of scene resources,
    // and a modifiable index pointing to one of those resources, spawn
    // a combo box for selecting said resource, where names are pulled using
    // a user-provided visitor function
    template <typename Ty>
    void push_resource_selector(const std::string                       &title,
                                rng::sized_range auto                   &range,
                                uint                                    &selection_j,
                                std::function<std::string (const Ty &)> name_visitor) {
      if (ImGui::BeginCombo(title.c_str(), name_visitor(range[selection_j]).c_str())) {
        for (uint i = 0; i < range.size(); ++i)
          if (ImGui::Selectable(name_visitor(range[i]).c_str(), selection_j == i)) {
            selection_j = i;
          }
        ImGui::EndCombo();
      } // if (BeginCombo)
    }
  } // namespace detail

  // Helper method;  spawn a sensible editor view with name editing, activity flags, a delete
  // button, and a default visitor for editing a scene component/resource's contained data
  template <typename Ty> requires (is_scene_data<Ty>)
  void push_editor(SchedulerHandle             &info, 
                   uint                         data_i,
                   detail::ImGuiEditInfo        edit_info = {},
                   detail::ImGuiEditVisitor<Ty> visitor   = detail::edit_visitor_default<Ty>) {
    met_trace();

    // Set local scope ID j.i.c.
    auto _scope = ImGui::ScopedID(fmt::format("{}_edit_{}", typeid(Ty).name(), data_i));

    // Get external resources and shorthands
    const auto &scene = info.global("scene").getr<Scene>();
    const auto &data  = scene_data_by_type<Ty>(scene)[data_i];

    // If requested, spawn a TreeNode.
    auto section_flag = edit_info.default_open ? ImGuiTreeNodeFlags_DefaultOpen : ImGuiTreeNodeFlags_None;
    bool section_open = !edit_info.inside_tree || ImGui::TreeNodeEx(data.name.c_str(), section_flag);

    // Is_active button, on same line as tree node if available
    if constexpr (detail::has_active_value<typename Ty::value_type>) {
      if (edit_info.inside_tree && edit_info.edit_data) {
        ImGui::SameLine(ImGui::GetContentRegionMax().x - 38.f);
        encapsulate_scene_data<Ty>(info, data_i, [](auto &info, uint i, auto &data) {
          if (ImGui::SmallButton(data.value.is_active ? "V" : "H"))
            data.value.is_active = !data.value.is_active;
          if (ImGui::IsItemHovered())
            ImGui::SetTooltip("Toggle component (in)active");
        });
      } // if (inside_tree && edit_data)
    } // if (detail::has_active_value)

    // Duplicate button, on same line as tree node if available
    if (edit_info.inside_tree && edit_info.show_dupl) {
      ImGui::SameLine(ImGui::GetContentRegionMax().x - 60.f);
      if (ImGui::SmallButton("D")) {
        if (section_open && edit_info.inside_tree) ImGui::TreePop();
        info.global("scene").getw<Scene>().touch({
          .name = "Duplicate component",
          .redo = [data] (auto &scene)                             { 
            scene_data_by_type<Ty>(scene).push_back(data); },
          .undo = [](auto &scene)                                  { 
            scene_data_by_type<Ty>(scene).pop_back();      }
        });
        return; // Exit early as iterators are invalidated
      }
      if (ImGui::IsItemHovered())
        ImGui::SetTooltip("Duplicate component");
    } // if (inside_tree && show_dupl)

    // Delete button, on same line as tree node if available
    if (edit_info.inside_tree && edit_info.show_del) {
      ImGui::SameLine(ImGui::GetContentRegionMax().x - 16.f);
      if (ImGui::SmallButton("X")) {
        if (section_open && edit_info.inside_tree) ImGui::TreePop();
        info.global("scene").getw<Scene>().touch({
          .name = "Delete component",
          .redo = [data_i] (auto &scene)                                { 
            scene_data_by_type<Ty>(scene).erase(data_i);        },
          .undo = [data_i, data](auto &scene)                           { 
            scene_data_by_type<Ty>(scene).insert(data_i, data); }
        });
        return; // Exit early as iterators are invalidated
      }
      if (ImGui::IsItemHovered())
        ImGui::SetTooltip("Delete component");
    } // if (inside_tree && show_del)

    // If the section is closed, we can now return early
    guard(section_open);
    
    // Optionally spawn component name editor
    if (edit_info.edit_name) {
      encapsulate_scene_name<Ty>(info, data_i, [](auto &info, auto &name) {
        constexpr static auto str_edit_flags = ImGuiInputTextFlags_AutoSelectAll 
                                             | ImGuiInputTextFlags_EnterReturnsTrue;
        return ImGui::InputText("Name", &name, str_edit_flags);
      });
    }
    
    // Encapsulate component visitor, to safely run value editing code
    if (edit_info.edit_data)
      encapsulate_scene_data<Ty>(info, data_i, visitor);
    
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
    auto _scope = ImGui::ScopedID(fmt::format("{}_list", typeid(Ty).name()));
    
    // Get external resources and shorthands
    const auto &scene  = info.global("scene").getr<Scene>();
    const auto &data  = scene_data_by_type<Ty>(scene);
    
    // If requested, spawn a TreeNode
    bool section_open = !edit_info.inside_tree || ImGui::CollapsingHeader(edit_info.editor_name.c_str(), ImGuiTreeNodeFlags_DefaultOpen);

    // If the section is closed, we can now return early
    guard(section_open);

    // Iterate over all relevant components
    for (uint i = 0; i < data.size(); ++i) {
      guard_break(i < data.size()); // Gracefully handle a deletion

      // If inlining, add a visual separator between each component
      if (i > 0 && !edit_info.inside_tree)
        ImGui::Separator();
      
      // Visit component internals
      push_editor<Ty>(info, i, edit_info, visitor);
    } // for (uint i)

    // Handle creation of new components
    if (edit_info.show_add) {
      // If there are listed components, visually separate this section
      if (!data.empty())
        ImGui::Separator();
      
      // Spawn an addition button to the right
      ImGui::NewLine();
      ImGui::SameLine(ImGui::GetContentRegionMax().x - 32.f);
      if (ImGui::SmallButton("Add")) {
        info.global("scene").getw<Scene>().touch({
          .name = "Add component",
          .redo = [](auto &scene) {
            scene_data_by_type<Ty>(scene).push("New component", { }); },
          .undo = [](auto &scene) {
            scene_data_by_type<Ty>(scene).erase(scene_data_by_type<Ty>(scene).size() - 1); }
        });
      }
    }
  }
} // namespace met