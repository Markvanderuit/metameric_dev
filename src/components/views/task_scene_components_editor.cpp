#include <metameric/components/views/task_scene_components_editor.hpp>
#include <metameric/components/views/detail/imgui.hpp>
#include <format>

namespace met {
  namespace detail {
    constexpr
    auto fun_resource_selector = [](std::string_view title, const auto &resources, uint &j) {
      if (ImGui::BeginCombo(title.data(), resources[j].name.c_str())) {
        for (uint i = 0; i < resources.size(); ++i)
          if (ImGui::Selectable(resources[i].name.c_str(), j == i))
            j = i;
        ImGui::EndCombo();
      } // if (BeginCombo)
    };

    std::string str_from_constraint_type(Uplifting::Constraint::Type type) {
      using Type = Uplifting::Constraint::Type;
      switch (type) {
        case Type::eColor:       return "Color";
        case Type::eColorOnMesh: return "Color (on mesh)";
        case Type::eMeasurement: return "Measurement";
        default:                 return "Unknown";
      };
    }
  } // namespace detail

  void SceneComponentsEditorTask::eval(SchedulerHandle &info) {
    met_trace_full();

    if (ImGui::Begin("Scene components")) {
      eval_objects(info);
      eval_emitters(info);
      eval_upliftings(info);
      eval_colr_systems(info);
    }
    ImGui::End();
  }

  void SceneComponentsEditorTask::eval_objects(SchedulerHandle &info) {
    met_trace_full();

    // Get external resources and shorthands
    const auto &e_handler    = info.global("scene_handler").read_only<SceneHandler>();
    const auto &e_scene      = e_handler.scene;
    const auto &e_objects    = e_scene.components.objects;
    const auto &e_upliftings = e_scene.components.upliftings;
    const auto &e_meshes     = e_scene.resources.meshes;
    const auto &e_images     = e_scene.resources.images;

    // Spawn a collapsing header
    guard(ImGui::CollapsingHeader(std::format("Objects ({})", e_objects.size()).c_str(), ImGuiTreeNodeFlags_DefaultOpen));
    ImGui::PushID("object_data");

    // Iterate over all objects
    for (uint i = 0; i < e_objects.size(); ++i) {
      guard_break(i < e_objects.size()); // Gracefully handle a deletion
      
      ImGui::PushID(std::format("object_data_{}", i).c_str());
      
      // We copy the object, and then test for changes
      const auto &component = e_objects[i];
            auto object     = component.value;

      // Add treenode section; postpone jumping into section
      bool open_section = ImGui::TreeNodeEx(component.name.c_str());
      
      // Insert delete button, is_active button on same
      ImGui::SameLine(ImGui::GetContentRegionMax().x - 38.f);
      if (ImGui::SmallButton(object.is_active ? "V" : "H"))
        object.is_active = !object.is_active;
      ImGui::SameLine(ImGui::GetContentRegionMax().x - 16.f);
      if (ImGui::SmallButton("X")) {
        info.global("scene_handler").writeable<SceneHandler>().touch({
          .name = "Delete object",
          .redo = [i = i]        (auto &scene) { scene.components.objects.erase(i); },
          .undo = [o = e_objects](auto &scene) { scene.components.objects = o;      }
        });
        break;
      }

      if (open_section) {
        // Object mesh/uplifting selection
        detail::fun_resource_selector("Uplifting", e_upliftings, object.uplifting_i);
        detail::fun_resource_selector("Mesh", e_meshes, object.mesh_i);

        ImGui::Separator();

        // Diffuse section
        ImGui::SetNextItemWidth(ImGui::GetContentRegionAvail().x * 0.25);
        if (ImGui::BeginCombo("##diffuse_data", "Diffuse")) {
          if (ImGui::Selectable("Value", object.diffuse.index() == 0))
            object.diffuse.emplace<0>(Colr(1));
          if (ImGui::Selectable("Texture", object.diffuse.index() == 1))
            object.diffuse.emplace<1>(0u);
          ImGui::EndCombo();
        } // If (BeginCombo)
        ImGui::SameLine();
        ImGui::SetNextItemWidth(ImGui::GetContentRegionAvail().x);
        if (object.diffuse.index() == 0) {
          ImGui::ColorEdit3("##diffuse_value", std::get<0>(object.diffuse).data());
        } else {
          detail::fun_resource_selector("##diffuse_texture", e_images, std::get<1>(object.diffuse));
        }
        
        // Roughess section
        ImGui::SetNextItemWidth(ImGui::GetContentRegionAvail().x * 0.25);
        if (ImGui::BeginCombo("##roughness_data", "Roughness")) {
          if (ImGui::Selectable("Value", object.roughness.index() == 0))
            object.roughness.emplace<0>(0.f);
          if (ImGui::Selectable("Texture", object.roughness.index() == 1))
            object.roughness.emplace<1>(0u);
          ImGui::EndCombo();
        } // If (BeginCombo)
        ImGui::SameLine();
        ImGui::SetNextItemWidth(ImGui::GetContentRegionAvail().x);
        if (object.roughness.index() == 0) {
          ImGui::SliderFloat("##roughness_value", &std::get<0>(object.roughness), 0.f, 1.f);
        } else {
          detail::fun_resource_selector("##roughness_texture", e_images, std::get<1>(object.roughness));
        }
        
        // Metallic section
        ImGui::SetNextItemWidth(ImGui::GetContentRegionAvail().x * 0.25);
        if (ImGui::BeginCombo("##metallic_data", "Metallic")) {
          if (ImGui::Selectable("Value", object.metallic.index() == 0))
            object.roughness.emplace<0>(0.f);
          if (ImGui::Selectable("Texture", object.metallic.index() == 1))
            object.metallic.emplace<1>(0u);
          ImGui::EndCombo();
        } // If (BeginCombo)
        ImGui::SameLine();
        ImGui::SetNextItemWidth(ImGui::GetContentRegionAvail().x);
        if (object.metallic.index() == 0) {
          ImGui::SliderFloat("##metallic_value", &std::get<0>(object.metallic), 0.f, 1.f);
        } else {
          detail::fun_resource_selector("##metallic_texture", e_images, std::get<1>(object.metallic));
        }
        
        // Opacity section
        ImGui::SetNextItemWidth(ImGui::GetContentRegionAvail().x * 0.25);
        if (ImGui::BeginCombo("##opacity_data", "Opacity")) {
          if (ImGui::Selectable("Value", object.opacity.index() == 0))
            object.roughness.emplace<0>(0.f);
          if (ImGui::Selectable("Texture", object.opacity.index() == 1))
            object.opacity.emplace<1>(0u);
          ImGui::EndCombo();
        } // If (BeginCombo)
        ImGui::SameLine();
        ImGui::SetNextItemWidth(ImGui::GetContentRegionAvail().x);
        if (object.opacity.index() == 0) {
          ImGui::SliderFloat("##opacity_value", &std::get<0>(object.opacity), 0.f, 1.f);
        } else {
          detail::fun_resource_selector("##opacity_texture", e_images, std::get<1>(object.opacity));
        }
        
        ImGui::TreePop();
      } // if (open_section)

      // Handle modifications to object copy
      if (object != component.value) {
        info.global("scene_handler").writeable<SceneHandler>().touch({
          .name = "Modify object",
          .redo = [i = i, obj = object         ](auto &scene) { scene.components.objects[i].value = obj; },
          .undo = [i = i, obj = component.value](auto &scene) { scene.components.objects[i].value = obj; }
        });
      }

      ImGui::PopID();
    } // for (uint i)
    
    ImGui::PopID();
  }

  void SceneComponentsEditorTask::eval_emitters(SchedulerHandle &info) {
    met_trace_full();

    // Get external resources
    const auto &e_handler     = info.global("scene_handler").read_only<SceneHandler>();
    const auto &e_scene       = e_handler.scene;
    const auto &e_emitters    = e_scene.components.emitters;
    const auto &e_illuminants = e_scene.resources.illuminants;

    // Spawn a collapsing header section
    guard(ImGui::CollapsingHeader(std::format("Emitters ({})", e_emitters.size()).c_str(), ImGuiTreeNodeFlags_DefaultOpen));
    ImGui::PushID("emitter_data");

    // Iterate over all objects
    for (uint i = 0; i < e_emitters.size(); ++i) {
      guard_break(i < e_emitters.size()); // Gracefully handle a deletion

      ImGui::PushID(std::format("emitter_data_{}", i).c_str());

      // We copy the emitter, and then test for changes with the component's orignal data later
      const auto &component = e_emitters[i];
            auto emitter    = component.value;

      // Add treenode section; postpone jumping into section
      bool open_section = ImGui::TreeNodeEx(component.name.c_str());
      
      // Insert delete button, is_active button on same
      ImGui::SameLine(ImGui::GetContentRegionMax().x - 38.f);
      if (ImGui::SmallButton(emitter.is_active ? "V" : "H"))
        emitter.is_active = !emitter.is_active;
      ImGui::SameLine(ImGui::GetContentRegionMax().x - 16.f);
      if (ImGui::SmallButton("X")) {
        info.global("scene_handler").writeable<SceneHandler>().touch({
          .name = "Delete emitter",
          .redo = [i = i]         (auto &scene) { scene.components.emitters.erase(i); },
          .undo = [o = e_emitters](auto &scene) { scene.components.emitters = o;      }
        });
        break;
      }

      if (open_section) {
        detail::fun_resource_selector("Illuminant", e_illuminants, emitter.illuminant_i);
        ImGui::SliderFloat("Power", &emitter.multiplier, 0.f, 10.f);
        ImGui::InputFloat3("Position", emitter.p.data());
        
        ImGui::TreePop();
      } // if (open_section)

      // Handle modifications to emitter copy
      if (emitter != component.value) {
        info.global("scene_handler").writeable<SceneHandler>().touch({
          .name = "Modify emitter",
          .redo = [i = i, obj = emitter        ](auto &scene) { scene.components.emitters[i].value = obj; },
          .undo = [i = i, obj = component.value](auto &scene) { scene.components.emitters[i].value = obj; }
        });
      }

      ImGui::PopID();
    } // for (uint i)
    
    ImGui::PopID();
  }
    
  void SceneComponentsEditorTask::eval_upliftings(SchedulerHandle &info) {
    met_trace_full();

    // Get external resources and shorthands
    const auto &e_handler    = info.global("scene_handler").read_only<SceneHandler>();
    const auto &e_scene      = e_handler.scene;
    const auto &e_upliftings = e_scene.components.upliftings;

    // Spawn a collapsing header section
    guard(ImGui::CollapsingHeader(std::format("Upliftings ({})", e_upliftings.size()).c_str(), ImGuiTreeNodeFlags_DefaultOpen));
    ImGui::PushID("uplifting_data");

    // Iterate over all objects
    for (uint i = 0; i < e_upliftings.size(); ++i) {
      guard_break(i < e_upliftings.size()); // Gracefully handle a deletion
      
      ImGui::PushID(std::format("object_data_{}", i).c_str());
      
      // We copy the object, and then test for changes
      // TODO; should handle this fine-grained for (large) upliftings
      const auto &component = e_upliftings[i];
            auto  uplifting = component.value;
      
      // Add treenode section; postpone jumping into section
      bool open_section = ImGui::TreeNodeEx(component.name.c_str());
      
      // Insert delete button, is_active button on same
      ImGui::SameLine(ImGui::GetContentRegionMax().x - 16.f);
      if (ImGui::SmallButton("X")) {
        info.global("scene_handler").writeable<SceneHandler>().touch({
          .name = "Delete uplifting",
          .redo = [i = i]           (auto &scene) { scene.components.upliftings.erase(i); },
          .undo = [o = e_upliftings](auto &scene) { scene.components.upliftings = o;      }
        });
        break;
      }

      if (open_section) {
        detail::fun_resource_selector("Basis", e_scene.resources.bases, uplifting.basis_i);


        for (auto &vert : uplifting.verts) {
          ImGui::Separator();
          ImGui::Indent();

          auto type_str = detail::str_from_constraint_type(vert.type);
          ImGui::LabelText("Type", type_str.c_str());

          

          ImGui::Unindent();
        } // for (vert)       

        ImGui::TreePop();
      } // if (open_section)

      ImGui::PopID();
    } // for (uint i)

    ImGui::PopID();
  }

  void SceneComponentsEditorTask::eval_colr_systems(SchedulerHandle &info) {
    met_trace_full();

    // Get external resources and shorthands
    const auto &e_handler      = info.global("scene_handler").read_only<SceneHandler>();
    const auto &e_scene        = e_handler.scene;
    const auto &e_colr_systems = e_scene.components.colr_systems;

    // Spawn a collapsing header section
    guard(ImGui::CollapsingHeader(std::format("Color systems ({})", e_colr_systems.size()).c_str(), ImGuiTreeNodeFlags_DefaultOpen));
    ImGui::PushID("colr_system_data");

    for (const auto &csys : e_colr_systems) {
      if (ImGui::TreeNodeEx(csys.name.c_str(), ImGuiTreeNodeFlags_Leaf)) {
        ImGui::SameLine(ImGui::GetContentRegionMax().x - 16.f);

        if (ImGui::SmallButton("X")) {
          debug::check_expr(false, "Not implemented");
        } 
        
        ImGui::TreePop();
      }
    } // for (csys)

    ImGui::PopID();
  }
} // namespace met