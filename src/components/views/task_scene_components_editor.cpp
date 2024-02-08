#include <metameric/core/scene.hpp>
#include <metameric/components/views/task_scene_components_editor.hpp>
#include <metameric/components/views/detail/imgui.hpp>
#include <metameric/components/views/detail/component_edit.hpp>
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

    std::string str_from_uplifting_vertex_type(Uplifting::Vertex vert) {
      return std::visit([](auto &&arg) {
        using T = std::decay_t<decltype(arg)>;
        if constexpr (std::is_same_v<T, DirectColorConstraint>)
          return "direct";
        else if constexpr (std::is_same_v<T, MeasurementConstraint>)
          return "measurement";
        else if constexpr (std::is_same_v<T, DirectSurfaceConstraint>)
          return "direct surface";
        else if constexpr (std::is_same_v<T, IndirectSurfaceConstraint>)
          return "indirect surface";
        else
          return "unknown";
      }, vert.constraint);
    }

    std::string str_from_emitter_type(Emitter::Type type) {
      using Type = Emitter::Type;
      switch (type) {
        case Type::eConstant: return "Constant";
        case Type::ePoint:    return "Point";
        case Type::eRect:     return "Rectangle";
        case Type::eSphere:   return "Spherical";
        default:              return "Unknown";
      };
    }
  } // namespace detail

  void SceneComponentsEditorTask::eval(SchedulerHandle &info) {
    met_trace();

    if (ImGui::Begin("Scene components")) {
      eval_objects(info);
      eval_emitters(info);
      eval_upliftings(info);
      eval_colr_systems(info);
    }
    ImGui::End();
  }

  void SceneComponentsEditorTask::eval_objects(SchedulerHandle &info) {
    met_trace();

    // Get external resources and shorthands
    const auto &e_scene      = info.global("scene").getr<Scene>();
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
      
      push_imgui_object_edit(info, i);


      // ImGui::PushID(std::format("object_data_{}", i).c_str());
      
      // // We copy the object, and then test for changes
      // const auto &component = e_objects[i];
      //       auto object     = component.value;

      // // Add treenode section; postpone jumping into section
      // bool open_section = ImGui::TreeNodeEx(component.name.c_str());
      
      // // Insert delete button, is_active button on same line
      // ImGui::SameLine(ImGui::GetContentRegionMax().x - 38.f);
      // if (ImGui::SmallButton(object.is_active ? "V" : "H"))
      //   object.is_active = !object.is_active;
      // ImGui::SameLine(ImGui::GetContentRegionMax().x - 16.f);
      // if (ImGui::SmallButton("X")) {
      //   info.global("scene").getw<Scene>().touch({
      //     .name = "Delete object",
      //     .redo = [i = i]               (auto &scene) { scene.components.objects.erase(i);     },
      //     .undo = [i = i, o = component](auto &scene) { scene.components.objects.insert(i, o); }
      //   });
      //   break;
      // }

      // if (open_section) {
      //   // Name editor
      //   std::string str = component.name;
      //   constexpr auto str_edit_flags = ImGuiInputTextFlags_AutoSelectAll | ImGuiInputTextFlags_EnterReturnsTrue;
      //   if (ImGui::InputText("Name", &str, str_edit_flags)) {
      //     info.global("scene").getw<Scene>().touch({
      //       .name = "Modify object name",
      //       .redo = [i = i, str = str           ](auto &scene) { scene.components.objects[i].name = str; },
      //       .undo = [i = i, str = component.name](auto &scene) { scene.components.objects[i].name = str; }
      //     });
      //   }
        
      //   // Object mesh/uplifting selection
      //   detail::fun_resource_selector("Uplifting", e_upliftings, object.uplifting_i);
      //   detail::fun_resource_selector("Mesh", e_meshes, object.mesh_i);

      //   ImGui::Separator();

      //   // Object transforms
      //   ImGui::DragFloat3("Position", object.transform.position.data(), 0.01f, -100.f, 100.f);
      //   ImGui::DragFloat3("Rotation", object.transform.rotation.data(), 0.01f, -10.f, 10.f);
      //   ImGui::DragFloat3("Scaling",  object.transform.scaling.data(),  0.01f, 0.001f, 100.f);

      //   // Important catch; prevent scale from falling to 0, something somewhere breaks :D
      //   object.transform.scaling = object.transform.scaling.cwiseMax(0.001f);
        
      //   ImGui::Separator();

      //   // Diffuse section
      //   ImGui::SetNextItemWidth(ImGui::GetContentRegionAvail().x * 0.75);
      //   if (object.diffuse.index() == 0) {
      //     ImGui::ColorEdit3("##diffuse_value", std::get<0>(object.diffuse).data());
      //   } else {
      //     detail::fun_resource_selector("##diffuse_texture", e_images, std::get<1>(object.diffuse));
      //   }
      //   ImGui::SameLine();
      //   ImGui::SetNextItemWidth(ImGui::GetContentRegionAvail().x);
      //   if (ImGui::BeginCombo("##diffuse_data", "Diffuse")) {
      //     if (ImGui::Selectable("Value", object.diffuse.index() == 0))
      //       object.diffuse.emplace<0>(Colr(1));
      //     if (ImGui::Selectable("Texture", object.diffuse.index() == 1))
      //       object.diffuse.emplace<1>(0u);
      //     ImGui::EndCombo();
      //   } // If (BeginCombo)
        
      //   /* // Roughess section
      //   ImGui::SetNextItemWidth(ImGui::GetContentRegionAvail().x * 0.75);
      //   if (object.roughness.index() == 0) {
      //     ImGui::SliderFloat("##roughness_value", &std::get<0>(object.roughness), 0.f, 1.f);
      //   } else {
      //     detail::fun_resource_selector("##roughness_texture", e_images, std::get<1>(object.roughness));
      //   }
      //   ImGui::SameLine();
      //   ImGui::SetNextItemWidth(ImGui::GetContentRegionAvail().x);
      //   if (ImGui::BeginCombo("##roughness_data", "Roughness")) {
      //     if (ImGui::Selectable("Value", object.roughness.index() == 0))
      //       object.roughness.emplace<0>(0.f);
      //     if (ImGui::Selectable("Texture", object.roughness.index() == 1))
      //       object.roughness.emplace<1>(0u);
      //     ImGui::EndCombo();
      //   } // If (BeginCombo) */

      //   /* // Metallic section
      //   ImGui::SetNextItemWidth(ImGui::GetContentRegionAvail().x * 0.75);
      //   if (object.metallic.index() == 0) {
      //     ImGui::SliderFloat("##metallic_value", &std::get<0>(object.metallic), 0.f, 1.f);
      //   } else {
      //     detail::fun_resource_selector("##metallic_texture", e_images, std::get<1>(object.metallic));
      //   }
      //   ImGui::SameLine();
      //   ImGui::SetNextItemWidth(ImGui::GetContentRegionAvail().x);
      //   if (ImGui::BeginCombo("##metallic_data", "Metallic")) {
      //     if (ImGui::Selectable("Value", object.metallic.index() == 0))
      //       object.roughness.emplace<0>(0.f);
      //     if (ImGui::Selectable("Texture", object.metallic.index() == 1))
      //       object.metallic.emplace<1>(0u);
      //     ImGui::EndCombo();
      //   } // If (BeginCombo) */

      //  /*  // Opacity section
      //   ImGui::SetNextItemWidth(ImGui::GetContentRegionAvail().x * 0.75);
      //   if (object.opacity.index() == 0) {
      //     ImGui::SliderFloat("##opacity_value", &std::get<0>(object.opacity), 0.f, 1.f);
      //   } else {
      //     detail::fun_resource_selector("##opacity_texture", e_images, std::get<1>(object.opacity));
      //   }
      //   ImGui::SameLine();
      //   ImGui::SetNextItemWidth(ImGui::GetContentRegionAvail().x);
      //   if (ImGui::BeginCombo("##opacity_data", "Opacity")) {
      //     if (ImGui::Selectable("Value", object.opacity.index() == 0))
      //       object.roughness.emplace<0>(0.f);
      //     if (ImGui::Selectable("Texture", object.opacity.index() == 1))
      //       object.opacity.emplace<1>(0u);
      //     ImGui::EndCombo();
      //   } // If (BeginCombo) */
        
      //   ImGui::TreePop();
      // } // if (open_section)

      // // Handle modifications to object copy
      // if (object != component.value) {
      //   info.global("scene").getw<Scene>().touch({
      //     .name = "Modify object",
      //     .redo = [i = i, obj = object         ](auto &scene) { scene.components.objects[i].value = obj; },
      //     .undo = [i = i, obj = component.value](auto &scene) { scene.components.objects[i].value = obj; }
      //   });
      // }

      // ImGui::PopID();
    } // for (uint i)

    // Handle additions to objects
    {
      if (!e_objects.empty())
        ImGui::Separator();
      ImGui::NewLine();
      ImGui::SameLine(ImGui::GetContentRegionMax().x - 32.f);
      if (ImGui::SmallButton("Add")) {
        info.global("scene").getw<Scene>().touch({
          .name = "Add object",
          .redo = [](auto &scene) { scene.components.objects.push("Object", { });                        },
          .undo = [](auto &scene) { scene.components.objects.erase(scene.components.objects.size() - 1); }
        });
      }
    }
    
    ImGui::PopID();
  }

  void SceneComponentsEditorTask::eval_emitters(SchedulerHandle &info) {
    met_trace();

    // Get external resources
    const auto &e_scene       = info.global("scene").getr<Scene>();
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
        info.global("scene").getw<Scene>().touch({
          .name = "Delete emitter",
          .redo = [i = i]               (auto &scene) { scene.components.emitters.erase(i);     },
          .undo = [i = i, o = component](auto &scene) { scene.components.emitters.insert(i, o); }
        });
        break;
      }

      if (open_section) {
        // Name editor
        std::string str = component.name;
        constexpr auto str_edit_flags = ImGuiInputTextFlags_AutoSelectAll | ImGuiInputTextFlags_EnterReturnsTrue;
        if (ImGui::InputText("Name", &str, str_edit_flags)) {
          info.global("scene").getw<Scene>().touch({
            .name = "Modify emitter name",
            .redo = [i = i, str = str           ](auto &scene) { scene.components.emitters[i].name = str; },
            .undo = [i = i, str = component.name](auto &scene) { scene.components.emitters[i].name = str; }
          });
        }

        // Type selector
        {
          auto curr_name = detail::str_from_emitter_type(emitter.type);
          if (ImGui::BeginCombo("Type", curr_name.c_str())) {
            for (uint i = 0; i < 4; ++i) {
              auto select_type = static_cast<Emitter::Type>(i);
              auto select_name = detail::str_from_emitter_type(select_type);
              if (ImGui::Selectable(select_name.c_str(), select_type == emitter.type))
                emitter.type = select_type;
            } // for (uint i)
            ImGui::EndCombo();
          } // if (BeginCombo)
        }

        ImGui::Separator();
        
        // Object transforms
        // Some parts are only available in part dependent on emitter type
        ImGui::DragFloat3("Position", emitter.transform.position.data(), 0.01f, -100.f, 100.f);
        if (emitter.type == Emitter::Type::eSphere) {
          ImGui::DragFloat("Scaling", emitter.transform.scaling.data(), 0.01f, 0.001f, 100.f);
          emitter.transform.scaling = eig::Vector3f(emitter.transform.scaling.x());
        } else if (emitter.type == Emitter::Type::eRect) {
          ImGui::DragFloat3("Rotation", emitter.transform.rotation.data(), 0.01f, -10.f, 10.f);
          ImGui::DragFloat2("Scaling", emitter.transform.scaling.data(), 0.01f, 0.001f, 100.f);
        }
        
        ImGui::Separator();

        detail::fun_resource_selector("Illuminant", e_illuminants, emitter.illuminant_i);
        ImGui::DragFloat("Power", &emitter.illuminant_scale, 0.1f, 0.0f, 100.f);

        ImGui::TreePop();
      } // if (open_section)

      // Handle modifications to emitter copy
      if (emitter != component.value) {
        info.global("scene").getw<Scene>().touch({
          .name = "Modify emitter",
          .redo = [i = i, obj = emitter        ](auto &scene) { scene.components.emitters[i].value = obj; },
          .undo = [i = i, obj = component.value](auto &scene) { scene.components.emitters[i].value = obj; }
        });
      }

      ImGui::PopID();
    } // for (uint i)

    // Handle additions to emitters
    {
      if (!e_emitters.empty())
        ImGui::Separator();
      ImGui::NewLine();
      ImGui::SameLine(ImGui::GetContentRegionMax().x - 32.f);
      if (ImGui::SmallButton("Add")) {
        info.global("scene").getw<Scene>().touch({
          .name = "Add emitter",
          .redo = [](auto &scene) { scene.components.emitters.push("Emitter, D65", { });                    },
          .undo = [](auto &scene) { scene.components.emitters.erase(scene.components.emitters.size() - 1); }
        });
      }
    }

    ImGui::PopID();
  }
    
  void SceneComponentsEditorTask::eval_upliftings(SchedulerHandle &info) {
    met_trace();

    // Get external resources and shorthands
    const auto &e_scene      = info.global("scene").getr<Scene>();
    const auto &e_objects    = e_scene.components.objects;
    const auto &e_upliftings = e_scene.components.upliftings;

    // Spawn a collapsing header section
    guard(ImGui::CollapsingHeader(std::format("Upliftings ({})", e_upliftings.size()).c_str(), ImGuiTreeNodeFlags_DefaultOpen));
    ImGui::PushID("uplifting_data");

    // Iterate over all upliftings
    for (uint i = 0; i < e_upliftings.size(); ++i) {
      guard_break(i < e_upliftings.size()); // Gracefully handle a deletion
      
      ImGui::PushID(std::format("uplifting_data_{}", i).c_str());
      
      // We copy the object, and then test for changes
      const auto &component = e_upliftings[i];
            auto  uplifting = component.value;
      
      // Add treenode section; postpone jumping into section
      bool open_section = ImGui::TreeNodeEx(component.name.c_str());
      
      // Insert delete button, is_active button on same
      ImGui::SameLine(ImGui::GetContentRegionMax().x - 16.f);
      if (ImGui::SmallButton("X")) {
        info.global("scene").getw<Scene>().touch({
          .name = "Delete uplifting",
          .redo = [i = i]               (auto &scene) { scene.components.upliftings.erase(i);     },
          .undo = [i = i, o = component](auto &scene) { scene.components.upliftings.insert(i, o); }
        });
        break;
      }

      if (open_section) {
        detail::fun_resource_selector("Basis", e_scene.resources.bases, uplifting.basis_i);
        
        // Iterate constraints
        for (uint j = 0; j < uplifting.verts.size(); ++j) {
          ImGui::PushID(std::format("uplifting_vertex_data_{}", j).c_str());

          auto &vert     = uplifting.verts[j];
          auto vert_name = std::format("Constraint {} ({})", j, detail::str_from_uplifting_vertex_type(vert));
          
          // Add treenode section; postpone jumping into section
          bool open_section = ImGui::TreeNodeEx(vert_name.c_str());

          // Insert delete button, is_active button on same line
          ImGui::SameLine(ImGui::GetContentRegionMax().x - 38.f);
          if (ImGui::SmallButton(vert.is_active ? "V" : "H"))
            vert.is_active = !vert.is_active;
          ImGui::SameLine(ImGui::GetContentRegionMax().x - 16.f);

          if (ImGui::SmallButton("X")) {
            uplifting.verts.erase(uplifting.verts.begin() + j);
            break;
          }

          if (open_section) {
            if (auto *constraint = std::get_if<DirectSurfaceConstraint>(&vert.constraint)) {
              ImGui::InputFloat3("Surface position", constraint->surface.p.data());
              ImGui::ColorEdit3("Surface diffuse", constraint->surface.diffuse.data(),
                ImGuiColorEditFlags_Float    |
                ImGuiColorEditFlags_NoOptions);
            } else {
              ImGui::Text("Not implemented!");
            }
            ImGui::TreePop();
          } // if (open_section)

          ImGui::PopID();
        } // for (uint j)
        
        // Handle additions to uplifting vertices
        {
          if (!uplifting.verts.empty())
            ImGui::Separator();
          
          ImGui::NewLine();
          ImGui::SameLine(ImGui::GetContentRegionMax().x - 84.f);
          if (ImGui::SmallButton("Add constraint"))
            ImGui::OpenPopup("popup_add_uplifting_vertex");
          
          int selected_type = -1;

          if (ImGui::BeginPopup("popup_add_uplifting_vertex")) {
            if (ImGui::Selectable("Direct"))
              uplifting.verts.push_back({ .constraint = DirectColorConstraint { .colr_i = 0.5  }});
              
            if (ImGui::Selectable("Measurement"))
              uplifting.verts.push_back({ .constraint = MeasurementConstraint { .measurement = 0.5  }});
            
            if (ImGui::Selectable("Direct surface"))
              uplifting.verts.push_back({ .constraint = DirectSurfaceConstraint()});

            ImGui::EndPopup();
          }
        } 

        ImGui::TreePop();
      } // if (open_section)

      // Handle modifications to uplifting copy
      if (uplifting != component.value) {
        info.global("scene").getw<Scene>().touch({
          .name = "Modify uplifting",
          .redo = [i = i, v = uplifting      ](auto &scene) { scene.components.upliftings[i].value = v; },
          .undo = [i = i, v = component.value](auto &scene) { scene.components.upliftings[i].value = v; }
        });
      }

      ImGui::PopID();
    } // for (uint i)

    // Handle additions to emitters
    {
      if (!e_upliftings.empty())
        ImGui::Separator();
      ImGui::NewLine();
      ImGui::SameLine(ImGui::GetContentRegionMax().x - 32.f);
      if (ImGui::SmallButton("Add")) {
        info.global("scene").getw<Scene>().touch({
          .name = "Add uplifting",
          .redo = [](auto &scene) { scene.components.upliftings.push("Uplifting", { });                        },
          .undo = [](auto &scene) { scene.components.upliftings.erase(scene.components.upliftings.size() - 1); }
        });
      }
    }

    ImGui::PopID();
  }

  void SceneComponentsEditorTask::eval_colr_systems(SchedulerHandle &info) {
    met_trace();

    // Get external resources and shorthands
    const auto &e_scene = info.global("scene").getr<Scene>();
    const auto &e_csys  = e_scene.components.colr_systems;
    const auto &e_cmfs  = e_scene.resources.observers;
    const auto &e_illm  = e_scene.resources.illuminants;

    // Spawn a collapsing header section
    guard(ImGui::CollapsingHeader(std::format("Color systems ({})", e_csys.size()).c_str(), ImGuiTreeNodeFlags_DefaultOpen));
    ImGui::PushID("csys_data");

    // Iterate over all color systems
    for (uint i = 0; i < e_csys.size(); ++i) {
      guard_break(i < e_csys.size()); // Gracefully handle a deletion

      ImGui::PushID(std::format("csys_data_{}", i).c_str());

      // We copy the object, and then test for changes
      const auto &component = e_csys[i];
            auto csys       = component.value;

      // Add treenode section; postpone jumping into section
      bool open_section = ImGui::TreeNodeEx(component.name.c_str());

      // Insert delete button on same line
      ImGui::SameLine(ImGui::GetContentRegionMax().x - 16.f);
      if (ImGui::SmallButton("X")) {
        info.global("scene").getw<Scene>().touch({
          .name = "Delete csys",
          .redo = [i = i]               (auto &scene) { scene.components.colr_systems.erase(i);     },
          .undo = [i = i, o = component](auto &scene) { scene.components.colr_systems.insert(i, o); }
        });
        break;
      }

      if (open_section) {
        // Note: no name editor; let's keep things simple
        // Color system cmfs/illuminant selectors
        detail::fun_resource_selector("CMFS", e_cmfs, csys.observer_i);
        detail::fun_resource_selector("Illuminant", e_illm, csys.illuminant_i);
        
        ImGui::TreePop();
      } // if (open_section)

      // Handle modifications to object copy
      if (csys != component.value) {
        info.global("scene").getw<Scene>().touch({
          .name = "Modify color system",
          .redo = [i = i, obj = csys           ](auto &scene) { 
            scene.components.colr_systems[i].value = obj;
            scene.components.colr_systems[i].name = scene.get_csys_name(i);
          },
          .undo = [i = i, obj = component.value](auto &scene) { 
            scene.components.colr_systems[i].value = obj;
            scene.components.colr_systems[i].name = scene.get_csys_name(i);
          }
        });
      }

      ImGui::PopID();
    } // for (uint i)

    // Handle additions to emitters
    {
      if (!e_csys.empty())
        ImGui::Separator();
      ImGui::NewLine();
      ImGui::SameLine(ImGui::GetContentRegionMax().x - 32.f);
      if (ImGui::SmallButton("Add")) {
        info.global("scene").getw<Scene>().touch({
          .name = "Add color system",
          .redo = [](auto &scene) { 
            ColorSystem csys { .observer_i = 0, .illuminant_i = 0, .n_scatters = 0 };
            scene.components.colr_systems.push(scene.get_csys_name(csys), csys);
          },
          .undo = [](auto &scene) { 
            scene.components.objects.erase(scene.components.objects.size() - 1); 
          }
        });
      }
    }

    ImGui::PopID();
  }
} // namespace met