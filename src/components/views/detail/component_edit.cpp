#include <metameric/core/scene.hpp>
#include <metameric/core/utility.hpp>
#include <metameric/components/views/detail/imgui.hpp>
#include <metameric/components/views/detail/component_edit.hpp>
#include <metameric/components/views/task_mmv_editor.hpp>
#include <concepts>

namespace met {
  namespace detail {
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

    // Default implementation of editing visitor for Object components
    template <>
    void edit_visitor_default(SchedulerHandle &info, uint i, Component<Object> &component) {
      // Get external resources and shorthands
      const auto &scene = info.global("scene").getr<Scene>();
      auto &value = component.value;

      // Object mesh/uplifting selectors
      push_resource_selector("Uplifting", scene.components.upliftings, value.uplifting_i);
      push_resource_selector("Mesh",      scene.resources.meshes, value.mesh_i);
      
      ImGui::Separator();

      // Object transforms
      ImGui::DragFloat3("Position", value.transform.position.data(), 0.01f, -100.f, 100.f);
      ImGui::DragFloat3("Rotation", value.transform.rotation.data(), 0.01f, -10.f, 10.f);
      ImGui::DragFloat3("Scaling",  value.transform.scaling.data(),  0.01f, 0.001f, 100.f);

      // Important catch; prevent scale from falling to 0, something somewhere breaks :D
      value.transform.scaling = value.transform.scaling.cwiseMax(0.001f);

      ImGui::Separator();
        
      // Texture selectors
      push_texture_variant_selector("Diffuse", scene.resources.images, value.diffuse);
      // push_texture_variant_selector("Roughness", scene.resources.images, value.roughness);
      // push_texture_variant_selector("Metallic", scene.resources.images, value.metallic);
      // push_texture_variant_selector("Normals", scene.resources.images, value.normals);
      // push_texture_variant_selector("Opacity", scene.resources.images, value.opacity);
    };

    // Default implementation of editing visitor for Emitter components
    template <>
    void edit_visitor_default(SchedulerHandle &info, uint i, Component<Emitter> &component) {
      // Get external resources and shorthands
      const auto &scene = info.global("scene").getr<Scene>();
      auto &value = component.value;
      
      // Type selector
      if (ImGui::BeginCombo("Type", std::format("{}", value.type).c_str())) {
        for (uint i = 0; i < 4; ++i) {
          auto type = static_cast<Emitter::Type>(i);
          auto name = std::format("{}", type);
          if (ImGui::Selectable(name.c_str(), value.type == type)) {
            value.type = type;
          }
        } // for (uint i)
        ImGui::EndCombo();
      }

      ImGui::Separator();

      // Object transforms
      // Some parts are only available in part dependent on emitter type
      ImGui::DragFloat3("Position", value.transform.position.data(), 0.01f, -100.f, 100.f);
      if (value.type == Emitter::Type::eSphere) {
        ImGui::DragFloat("Scaling", value.transform.scaling.data(), 0.01f, 0.001f, 100.f);
        value.transform.scaling = eig::Vector3f(value.transform.scaling.x());
      } else if (value.type == Emitter::Type::eRect) {
        ImGui::DragFloat3("Rotation", value.transform.rotation.data(), 0.01f, -10.f, 10.f);
        ImGui::DragFloat2("Scaling", value.transform.scaling.data(), 0.01f, 0.001f, 100.f);
      }
      
      ImGui::Separator();

      // Target distribution
      push_resource_selector("Illuminant", scene.resources.illuminants, value.illuminant_i);
      ImGui::DragFloat("Power", &value.illuminant_scale, 0.1f, 0.0f, 100.f);
    };

    // Default implementation of editing visitor for Uplifting components
    template <>
    void edit_visitor_default(SchedulerHandle &info, uint i, Component<Uplifting> &component) {
      // ImGui flag shorthands
      constexpr static auto str_edit_flags = ImGuiInputTextFlags_AutoSelectAll | ImGuiInputTextFlags_EnterReturnsTrue;
      constexpr static auto colr_view_flags = ImGuiColorEditFlags_Float | ImGuiColorEditFlags_NoOptions;
      
      // Get external resources and shorthands
      const auto &scene = info.global("scene").getr<Scene>();
      auto &value = component.value;
      
      // Uplifting value modifications
      push_resource_selector("Basis functions", scene.resources.bases, value.basis_i);
      push_resource_selector("Base color system", scene.components.colr_systems, value.csys_i);

      // Visual separator into constraint list
      ImGui::Separator();

      // Button and accompanying popup to add new constraint vertices
      if (ImGui::Button("New constraint")) {
        ImGui::OpenPopup("popup_add_uplifting_vertex");
      }
      if (ImGui::BeginPopup("popup_add_uplifting_vertex")) {
        if (ImGui::Selectable("Direct"))
          value.verts.push_back({ 
            .name       = "New direct constraint",
            .constraint = DirectColorConstraint { .colr_i = 0.5  }
          });
        if (ImGui::Selectable("Measurement"))
          value.verts.push_back({ 
            .name       = "New measurement constraint",
            .constraint = MeasurementConstraint { .measurement = 0.5  }
          });
        if (ImGui::Selectable("Direct surface"))
          value.verts.push_back({ 
            .name       = "New direct surface constraint",
            .constraint = DirectSurfaceConstraint()
          });
        ImGui::EndPopup();
      } // if (BeginPopup())

      // Button to clear existing constraints
      ImGui::SameLine();
      if (ImGui::Button("Clear constraints")) {
        value.verts.clear();
      }

      /* // Jump left to avoid treenode indentation
      ImGui::Unindent(ImGui::GetTreeNodeToLabelSpacing()); */
      
      // Vertex constraint type
      ImGui::BeginGroup();
      {
        ImGui::AlignTextToFramePadding();
        ImGui::SetNextItemWidth(ImGui::GetWindowWidth() * 0.25);
        ImGui::Text("Name");
        for (uint j = 0; j < value.verts.size(); ++j) {
          // Get shorthands
          auto _scope = ImGui::ScopedID(std::format("constraint_{}_{}", typeid(Uplifting::Vertex).name(), j));
          auto &vert  = value.verts[j];
          
          // Show name editor
          auto copy = vert.name; 
          ImGui::SetNextItemWidth(ImGui::GetWindowWidth() * 0.25);
          if (ImGui::InputText("##constraint_name", &copy, str_edit_flags)) {
            vert.name = copy;
          }
        } // for (uint j)
      }
      ImGui::EndGroup();
      ImGui::SameLine();

      // Vertex constraint type
      ImGui::BeginGroup();
      {
        ImGui::AlignTextToFramePadding();
        ImGui::SetNextItemWidth(ImGui::GetWindowWidth() * 0.25);
        ImGui::Text("Constraint");
        for (uint j = 0; j < value.verts.size(); ++j) {
          // Get shorthands
          auto _scope = ImGui::ScopedID(std::format("constraint_{}_{}", typeid(Uplifting::Vertex).name(), j));
          auto &vert  = value.verts[j];

          ImGui::SetNextItemWidth(ImGui::GetWindowWidth() * 0.25);
          if (ImGui::BeginCombo("##constraint_type", "Direct")) {
            ImGui::EndCombo();
          }
        } // for (uint j)
      }
      ImGui::EndGroup();
      ImGui::SameLine();

      // Vertex base color
      ImGui::BeginGroup();
      {
        ImGui::AlignTextToFramePadding();
        ImGui::Text("Result");
        for (uint j = 0; j < value.verts.size(); ++j) {
          // Get shorthands
          auto _scope = ImGui::ScopedID(std::format("basecolor_{}_{}", typeid(Uplifting::Vertex).name(), j));
          auto &vert  = value.verts[j];

          // Small 
          ImGui::ColorButton("##base_color", { 1, 0, 1, 1 }, ImGuiColorEditFlags_NoPicker | ImGuiColorEditFlags_Float);

          // Insert MMV editor button if vertex constraint supports this
          ImGui::SameLine();
          if (ImGui::Button("Edit")) {
            auto child_name   = std::format("mmv_editor_{}_{}", i, j);
            auto child_handle = info.child_task(child_name);
            if (!child_handle.is_init()) {
              child_handle.init<MMVEditorTask>(
                InputSelection { .uplifting_i = i, .constraint_i = j });
            }
          }
          if (ImGui::IsItemHovered())
            ImGui::SetTooltip("Edit constraint mismatching");
        } // for (uint j)
      }
      ImGui::EndGroup();
      ImGui::SameLine();

      // Vertex edit button
      ImGui::BeginGroup();
      {
        ImGui::AlignTextToFramePadding();
        if (ImGui::Button("Add constraint")) {
          
        }
        
        for (uint j = 0; j < value.verts.size(); ++j) {
          // Get shorthands
          auto _scope = ImGui::ScopedID(std::format("dadada_{}_{}", typeid(Uplifting::Vertex).name(), j));
          auto &vert  = value.verts[j];

          // Optionally insert hide/show button at end of line if vertex constraint supports this
          ImGui::NewLine();
          ImGui::Spacing();
          if constexpr (has_active_value<Uplifting::Vertex>) {
            if (ImGui::Button(vert.is_active ? "V" : "H"))
              vert.is_active = !vert.is_active;
            if (ImGui::IsItemHovered())
              ImGui::SetTooltip("Toggle active");
            ImGui::SameLine();
          }

          // Insert delete button at end of line
          if (ImGui::Button("X")) {
            value.verts.erase(value.verts.begin() + j);
            break;
          }
          if (ImGui::IsItemHovered())
            ImGui::SetTooltip("Delete component");
        } // for (uint j)
      }
      ImGui::EndGroup();

      // Per-constraint vertex, show a small tree node
      for (uint j = 0; j < value.verts.size(); ++j) {
        // Get shorthands
        auto _scope = ImGui::ScopedID(std::format("{}_{}", typeid(Uplifting::Vertex).name(), j));
        auto &vert  = value.verts[j];
        
        // Add treenode section; postpone jumping into section
        bool section_open = ImGui::TreeNodeEx(vert.name.c_str(), ImGuiTreeNodeFlags_DefaultOpen);
        
        // Optionally insert MMV editor spawn button if vertex constraint supports this
        if (vert.has_mismatching()) {
          ImGui::SameLine(ImGui::GetContentRegionMax().x - 104.f);
          if (ImGui::SmallButton("edit MMV")) {
            auto child_name   = std::format("mmv_editor_{}_{}", i, j);
            auto child_handle = info.child_task(child_name);
            if (!child_handle.is_init()) {
              child_handle.init<MMVEditorTask>(
                InputSelection { .uplifting_i = i, .constraint_i = j });
            }
          }
        }

        // Optionally insert hide/show button at end of line if vertex constraint supports this
        if constexpr (has_active_value<Uplifting::Vertex>) {
          ImGui::SameLine(ImGui::GetContentRegionMax().x - 38.f);
          if (ImGui::SmallButton(vert.is_active ? "V" : "H"))
            vert.is_active = !vert.is_active;
          if (ImGui::IsItemHovered())
            ImGui::SetTooltip("Toggle active");
        }

        // Insert delete button at end of line
        ImGui::SameLine(ImGui::GetContentRegionMax().x - 16.f);
        if (ImGui::SmallButton("X")) {
          if (section_open) ImGui::TreePop();
          value.verts.erase(value.verts.begin() + j);
          break;
        }
        if (ImGui::IsItemHovered())
          ImGui::SetTooltip("Delete component");

        // Continue if treenode is not open
        guard_continue(section_open);

        // Show name editor
        auto copy = vert.name; 
        if (ImGui::InputText("Name", &copy, str_edit_flags)) {
          vert.name = copy;
        }

        // Visitor handles the four different constraint types
        std::visit(overloaded {
          [&](DirectColorConstraint &c) {
            ImGui::ColorEdit3("Base color", c.colr_i.data(), colr_view_flags);

            // If requested, spawn mismatch volume editor
            // ImGui::Separator();
            /* if (ImGui::Button("Edit mismatching")) {
              auto child_name   = std::format("mmv_editor_{}_{}", i, j);
              auto child_handle = info.child_task(child_name);
              if (!child_handle.is_init()) {
                child_handle.init<MMVEditorTask>(
                  InputSelection { .uplifting_i = i, .constraint_i = j });
              }
            } */
          },
          [&](DirectSurfaceConstraint &c) {
            guard(c.is_valid());

            Colr lrgb = c.surface.diffuse;
            Colr srgb = lrgb_to_srgb(lrgb);

            ImGui::InputFloat3("Surface position", c.surface.p.data());
            ImGui::InputFloat3("Surface normal",   c.surface.n.data());
            
            ImGui::ColorEdit3("Diffuse color (lrgb)", lrgb.data(), colr_view_flags);
            ImGui::ColorEdit3("Diffuse color (srgb)", srgb.data(), colr_view_flags);

            // If requested, spawn mismatch volume editor
            // ImGui::Separator();
            /* if (ImGui::Button("Edit mismatching")) {
              auto child_name   = std::format("mmv_editor_{}_{}", i, j);
              auto child_handle = info.child_task(child_name);
              if (!child_handle.is_init()) {
                child_handle.init<MMVEditorTask>(
                  InputSelection { .uplifting_i = i, .constraint_i = j });
              }
            } */
          },
          [](auto &) {
            ImGui::Text("Not implemented");
          }
        }, vert.constraint);

        // Close treenode if the visitor got this far
        ImGui::TreePop();
      } // for (uint j)


      /* // Jump right to avoid treenode indentation
      ImGui::Indent(ImGui::GetTreeNodeToLabelSpacing()); */

      // Visual separator between vertex constraints and add button
      /* if (!value.verts.empty())
          ImGui::Separator(); */

      // Handle additions to vertex constraints
      /* ImGui::NewLine();
      ImGui::SameLine(ImGui::GetContentRegionMax().x - 84.f);
      if (ImGui::SmallButton("Add constraint"))
        ImGui::OpenPopup("popup_add_uplifting_vertex");
      int selected_type = -1;
      if (ImGui::BeginPopup("popup_add_uplifting_vertex")) {
        if (ImGui::Selectable("Direct"))
          value.verts.push_back({ 
            .name       = "New direct constraint",
            .constraint = DirectColorConstraint { .colr_i = 0.5  }
          });
        if (ImGui::Selectable("Measurement"))
          value.verts.push_back({ 
            .name       = "New measurement constraint",
            .constraint = MeasurementConstraint { .measurement = 0.5  }
          });
        if (ImGui::Selectable("Direct surface"))
          value.verts.push_back({ 
            .name       = "New direct surface constraint",
            .constraint = DirectSurfaceConstraint()
          });
        ImGui::EndPopup();
      } // if (BeginPopup()) */

    };

    // Default implementation of editing visitor for ColorSystem components
    template <>
    void edit_visitor_default(SchedulerHandle &info, uint i, Component<ColorSystem> &component) {
      // Get external resources and shorthands
      const auto &scene = info.global("scene").getr<Scene>();
      auto &value = component.value;
      
      push_resource_selector("CMFS", scene.resources.observers, value.observer_i);
      push_resource_selector("Illuminant", scene.resources.illuminants, value.illuminant_i);

      // Force update name to adhere to [CMFS][Illuminant] naming clarity
      component.name = scene.get_csys_name(value);
    };

    // Default implementation of editing visitor for Mesh resources
    template <>
    void edit_visitor_default(SchedulerHandle &info, uint i, const Resource<Mesh> &resource) {
      // Get external resources and shorthands
      const auto &scene = info.global("scene").getr<Scene>();
      const auto &value = resource.value();
      
      ImGui::LabelText("Vertices", "%d", value.verts.size());
      ImGui::LabelText("Elements", "%d", value.elems.size());
    };
  } // namespace detail

  /* Explicit template instantiations */
  template
  void push_editor<detail::Component<Object>>(SchedulerHandle &, detail::ImGuiEditInfo, detail::ImGuiEditVisitor<detail::Component<Object>>);
  template
  void push_editor<detail::Component<Emitter>>(SchedulerHandle &, detail::ImGuiEditInfo, detail::ImGuiEditVisitor<detail::Component<Emitter>>);
  template
  void push_editor<detail::Component<Uplifting>>(SchedulerHandle &, detail::ImGuiEditInfo, detail::ImGuiEditVisitor<detail::Component<Uplifting>>);
  template
  void push_editor<detail::Component<ColorSystem>>(SchedulerHandle &, detail::ImGuiEditInfo, detail::ImGuiEditVisitor<detail::Component<ColorSystem>>);
  template
  void push_editor<detail::Resource<Mesh>>(SchedulerHandle &, detail::ImGuiEditInfo, detail::ImGuiEditVisitor<detail::Resource<Mesh>>);
} // namespace met
