#include <metameric/core/scene.hpp>
#include <metameric/core/utility.hpp>
#include <metameric/components/views/detail/imgui.hpp>
#include <metameric/components/views/detail/component_edit.hpp>
#include <concepts>

namespace met::detail {
  // Helper function; forward to the matching scene components based on the specified type
  template <typename Ty>
  constexpr auto scene_component_accessor(Scene &scene) -> Components<Ty> & {
    if constexpr (std::is_same_v<Ty, ColorSystem>) {
      return scene.components.colr_systems;
    } else if constexpr (std::is_same_v<Ty, Emitter>) {
      return scene.components.emitters;
    } else if constexpr (std::is_same_v<Ty, Object>) {
      return scene.components.objects;
    } else if constexpr (std::is_same_v<Ty, Uplifting>) {
      return scene.components.upliftings;
    } else {
      debug::check_expr(false, "scene_component_accessor<Ty> exhausted its options"); 
    }
  }

  // Helper function; forward to the matching scene components based on the specified type
  template <typename Ty>
  constexpr auto scene_component_accessor(const Scene &scene) -> const Components<Ty> & {
    if constexpr (std::is_same_v<Ty, ColorSystem>) {
      return scene.components.colr_systems;
    } else if constexpr (std::is_same_v<Ty, Emitter>) {
      return scene.components.emitters;
    } else if constexpr (std::is_same_v<Ty, Object>) {
      return scene.components.objects;
    } else if constexpr (std::is_same_v<Ty, Uplifting>) {
      return scene.components.upliftings;
    } else {
      debug::check_expr(false, "scene_component_accessor<Ty> exhausted its options"); 
    }
  }

  // Helper function; safely encapsulate some value-editing inside scene change recording,
  // based on whether the value was changed in some way
  template <typename Ty>
  constexpr void encapsulate_value_editing(const std::string &edit_name, SchedulerHandle &info, uint i, std::function<void (SchedulerHandle &, Ty &)> visitor) {
    met_trace();

    // Access relevant resources
    // We copy the object, and then test for changes at the end of the section
    const auto &e_scene   = info.global("scene").getr<Scene>();
    const auto &component = scene_component_accessor<Ty>(e_scene)[i];
          auto value      = component.value;

    // Edit value
    visitor(info, value); // Visitor potentially edits value

    // Given inequality comparison, record a scene change for redo/undo
    if (value != component.value) {
      info.global("scene").getw<Scene>().touch({
        .name = edit_name,
        .redo = [i = i, v = value          ](auto &sc) { scene_component_accessor<Ty>(sc)[i].value = v; },
        .undo = [i = i, v = component.value](auto &sc) { scene_component_accessor<Ty>(sc)[i].value = v; }
      });
    }
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

  // Helper function; given a title, access to a set of textures, and a modifiable variant
  // representing a color or a texture, spawn a combo box for texture/color selection
  constexpr
  void push_texture_variant_selector(const std::string &title, const auto &resources, auto &variant) {
    // First, spawn a editor for the variant's specific type; color editor, or texture selector
    ImGui::SetNextItemWidth(ImGui::GetContentRegionAvail().x * 0.75);
    if (std::holds_alternative<Colr>(variant)) {
      ImGui::ColorEdit3(std::format("##_{}_value", title).c_str(), std::get<Colr>(variant).data());
    } else if (std::holds_alternative<uint>(variant)) {
      push_resource_selector(std::format("##_{}_txtr", title), resources, std::get<uint>(variant));
    }
    
    // Then, spawn a combobox to switch between the variant's types
    ImGui::SameLine();
    ImGui::SetNextItemWidth(ImGui::GetContentRegionAvail().x);
    if (ImGui::BeginCombo(std::format("##_{}_data", title).c_str(), title.c_str())) {
      if (ImGui::Selectable("Value", std::holds_alternative<Colr>(variant)))
        variant = Colr(1);
      if (ImGui::Selectable("Texture", std::holds_alternative<uint>(variant)))
        variant = uint(0u);
      ImGui::EndCombo();
    } // If (BeginCombo)
  }

  // Helper function; given a title and a specified component type and index,
  // enable name editing
  template <typename Ty>
  constexpr void push_name_editor(std::string_view title, SchedulerHandle &info, uint i) {
    met_trace();
    
    constexpr auto str_edit_flags = ImGuiInputTextFlags_AutoSelectAll | ImGuiInputTextFlags_EnterReturnsTrue;

    // We copy the component's name, and then test for changes
    const auto &e_scene   = info.global("scene").getr<Scene>();
    const auto &component = scene_component_accessor<Ty>(e_scene)[i];
          auto name       = component.name;

    // Spawn text editor; on a signaled edit, record a scene change for redo/undo
    if (ImGui::InputText(title.data(), &name, str_edit_flags)) {
      info.global("scene").getw<Scene>().touch({
        .name = "Modify component name",
        .redo = [i = i, name = name](auto &sc) { 
          scene_component_accessor<Ty>(sc)[i].name = name; },
        .undo = [i = i, name = component.name](auto &sc) { 
          scene_component_accessor<Ty>(sc)[i].name = name; }
      });
    }
  }

  template <typename Ty>
  concept has_active_value = requires (Ty t) {
    { t.is_active } -> std::same_as<bool>;
  };

  template <typename Ty>
  constexpr void push_active_button(SchedulerHandle &info, uint i) {
    met_trace();
    encapsulate_value_editing<Ty>("Swap component active", info, i, [](auto &info, auto &value) {
      if (ImGui::SmallButton(value.is_active ? "V" : "H"))
        value.is_active = !value.is_active;
    });
  }

  template <typename Ty>
  constexpr bool push_delete_button(SchedulerHandle &info, uint i) {
    met_trace();
    
    // Access relevant resources
    const auto &e_scene   = info.global("scene").getr<Scene>();
    const auto &component = scene_component_accessor<Ty>(e_scene)[i];

    // Spawn small X button
    if (ImGui::SmallButton("X")) {
      info.global("scene").getw<Scene>().touch({
        .name = "Delete component",
        .redo = [i = i] (auto &sc) { 
          scene_component_accessor<Ty>(sc).erase(i); },
        .undo = [i = i, o = component](auto &sc) { 
          scene_component_accessor<Ty>(sc).insert(i, o); }
      });
      return true;
    }
    return false;
  }

  template <typename Ty>
  void push_component_edit(SchedulerHandle &info, uint i, ImGuiEditInfo edit_info, std::function<void (SchedulerHandle &, Ty &)> visitor) {
    met_trace();

    // Set local scope ID j.i.c.
    auto _scope = ImGui::ScopedID(std::format("{}_edit_{}", typeid(Ty).name(), i));

    // Get external resources and shorthands
    const auto &e_scene   = info.global("scene").getr<Scene>();
    const auto &component = scene_component_accessor<Ty>(e_scene)[i];

    // If requested, spawn a TreeNode.
    bool section_open = !edit_info.inside_tree || ImGui::TreeNodeEx(component.name.c_str());

    // Is_active button, on same line as tree node if available
    if constexpr (has_active_value<Ty>) {
      if (edit_info.inside_tree && edit_info.enable_value_editing) {
        ImGui::SameLine(ImGui::GetContentRegionMax().x - 38.f);
        push_active_button<Ty>(info, i);
      } // if (inside_tree && enable_value_editing)
    } // if (has_active_value)

    // Delete button, on same line as tree node
    if (edit_info.inside_tree && edit_info.enable_delete) {
      ImGui::SameLine(ImGui::GetContentRegionMax().x - 16.f);
      guard(!push_delete_button<Ty>(info, i));
    } // if (inside_tree && enable_delete)

    //  If the section is closed, we return early
    guard(section_open);

    // Optionall spawn component name editor
    if (edit_info.enable_name_editing)
      push_name_editor<Ty>("Name", info, i);

    // Optionally internalize component data visitor
    if (edit_info.enable_value_editing)
      encapsulate_value_editing<Ty>("Modify object", info, i, visitor);

    // If the section needs closing, do so
    if (edit_info.inside_tree)
      ImGui::TreePop();
  }

  template <typename Ty>
  void push_components_edit(const std::string &section_name, SchedulerHandle &info, ImGuiEditInfo edit_info, std::function<void (SchedulerHandle &, Ty &)> visitor) {
    met_trace();

    // Set local scope ID j.i.c.
    auto _scope = ImGui::ScopedID(std::format("{}_list", typeid(Ty).name()));

    // Get external resources and shorthands
    const auto &e_scene      = info.global("scene").getr<Scene>();
    const auto &e_components = scene_component_accessor<Ty>(e_scene);

    // If requested, spawn a TreeNode. If the TreeNode is closed, return early
    bool section_open = !edit_info.inside_tree || ImGui::CollapsingHeader(section_name.c_str(), ImGuiTreeNodeFlags_DefaultOpen);

    // Iterate over all relevant components
    for (uint i = 0; i < e_components.size(); ++i) {
      guard_break(i < e_components.size()); // Gracefully handle a deletion

      // If inlining, add a visual separator between each component
      if (i > 0 && !edit_info.inside_tree)
        ImGui::Separator();
      
      // Visit component internals
      push_component_edit<Ty>(info, i, edit_info, visitor);
    } // for (uint i)

    if (edit_info.enable_addition) {
      // If there are listed components, visually separate this section
      if (!e_components.empty())
        ImGui::Separator();
      
      // Spawn an addition button
      ImGui::NewLine();
      ImGui::SameLine(ImGui::GetContentRegionMax().x - 32.f);

      if (ImGui::SmallButton("Add")) {
        info.global("scene").getw<Scene>().touch({
          .name = "Add component",
          .redo = [](auto &sc) { 
            scene_component_accessor<Ty>(sc).push("New component", { }); },
          .undo = [](auto &sc) { 
            scene_component_accessor<Ty>(sc).erase(scene_component_accessor<Ty>(sc).size() - 1); }
        });
      }
    }
  }

  // Default implementation of editing visitor for Object components
  constexpr auto object_visitor = [](SchedulerHandle &info, Object &value) {
    // Get external resources and shorthands
    const auto &e_scene = info.global("scene").getr<Scene>();

    // Object mesh/uplifting selectors
    push_resource_selector("Uplifting", e_scene.components.upliftings, value.uplifting_i);
    push_resource_selector("Mesh",      e_scene.resources.meshes, value.mesh_i);
    
    ImGui::Separator();

    // Object transforms
    ImGui::DragFloat3("Position", value.transform.position.data(), 0.01f, -100.f, 100.f);
    ImGui::DragFloat3("Rotation", value.transform.rotation.data(), 0.01f, -10.f, 10.f);
    ImGui::DragFloat3("Scaling",  value.transform.scaling.data(),  0.01f, 0.001f, 100.f);

    // Important catch; prevent scale from falling to 0, something somewhere breaks :D
    value.transform.scaling = value.transform.scaling.cwiseMax(0.001f);

    ImGui::Separator();
      
    // Texture selectors
    push_texture_variant_selector("Diffuse", e_scene.resources.images, value.diffuse);
    // push_texture_variant_selector("Roughness", e_scene.resources.images, value.roughness);
    // push_texture_variant_selector("Metallic", e_scene.resources.images, value.metallic);
    // push_texture_variant_selector("Normals", e_scene.resources.images, value.normals);
    // push_texture_variant_selector("Opacity", e_scene.resources.images, value.opacity);
  };


  // Default implementation of editing visitor for Emitter components
  constexpr auto emitter_visitor = [](SchedulerHandle &info, Emitter &value) {
    // Get external resources and shorthands
    const auto &e_scene = info.global("scene").getr<Scene>();
    
    // ...
  };

  // Default implementation of editing visitor for Uplifting components
  constexpr auto uplifting_visitor = [](SchedulerHandle &info, Uplifting &value) {
    // Get external resources and shorthands
    const auto &e_scene = info.global("scene").getr<Scene>();
    
    // ...
  };

  // Default implementation of editing visitor for ColorSystem components
  constexpr auto colr_system_visitor = [](SchedulerHandle &info, ColorSystem &value) {
    // Get external resources and shorthands
    const auto &e_scene = info.global("scene").getr<Scene>();
    
    // ...
  };

  void push_object_edit(SchedulerHandle &info, uint i, ImGuiEditInfo edit_info) {
    push_component_edit<Object>(info, i, edit_info, object_visitor);
  }

  void push_objects_edit(SchedulerHandle &info, ImGuiEditInfo edit_info) {
    push_components_edit<Object>("Objects", info, edit_info, object_visitor);
  }

  void push_emitter_edit(SchedulerHandle &info, uint i, ImGuiEditInfo edit_info) {
    push_component_edit<Emitter>(info, i, edit_info, emitter_visitor);
  }

  void push_emitters_edit(SchedulerHandle &info, ImGuiEditInfo edit_info) {
    push_components_edit<Emitter>("Emitters", info, edit_info, emitter_visitor);
  }

  void push_uplifting_edit(SchedulerHandle &info, uint i, ImGuiEditInfo edit_info) {
    push_component_edit<Uplifting>(info, i, edit_info, uplifting_visitor);
  }

  void push_upliftings_edit(SchedulerHandle &info, ImGuiEditInfo edit_info) {
    push_components_edit<Uplifting>("Upliftings", info, edit_info, uplifting_visitor);
  }

  void push_colr_system_edit(SchedulerHandle &info, uint i, ImGuiEditInfo edit_info) {
    push_component_edit<ColorSystem>(info, i, edit_info, colr_system_visitor);
  }

  void push_colr_systems_edit(SchedulerHandle &info, ImGuiEditInfo edit_info) {
    push_components_edit<ColorSystem>("Color Systems", info, edit_info, colr_system_visitor);
  }
} // namespace met::detail
