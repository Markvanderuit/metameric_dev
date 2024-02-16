#include <metameric/core/scene.hpp>
#include <metameric/core/utility.hpp>
#include <metameric/components/views/detail/imgui.hpp>
#include <metameric/components/views/detail/component_edit.hpp>
#include <metameric/components/views/task_mmv_editor.hpp>
#include <concepts>

namespace met {
  namespace detail {
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

      // Helper lambda to dedicate a group to all available vertices
      auto visit_vertex_column = [&](std::string                                            col_name, 
                                     float                                                  col_width,
                                     std::function<void (float width, Uplifting::Vertex &)> visitor) {
        ImGui::BeginGroup();
        ImGui::AlignTextToFramePadding();
        ImGui::SetNextItemWidth(ImGui::GetWindowWidth() * col_width);
        ImGui::Text(col_name.c_str());
        for (uint j = 0; j < value.verts.size(); ++j) {
          auto scope = ImGui::ScopedID(std::format("{}", j));
          visitor(col_width, value.verts[j]);
        }
        ImGui::EndGroup();
      };
      
      // Vertex name editor column
      detail::visit_range_column<Uplifting::Vertex>("Name", 0.25, value.verts, [&](uint i, Uplifting::Vertex &vert) {
        auto copy = vert.name; 
        ImGui::SetNextItemWidth(ImGui::GetWindowWidth() * 0.25);
        if (ImGui::InputText("##constraint_name", &copy, str_edit_flags)) {
          vert.name = copy;
        }
      });
      ImGui::SameLine();
      
      // Vertex type editor column;
      // if type is changed, constraint data is essentially discarded
      detail::visit_range_column<Uplifting::Vertex>("Type", 0.25, value.verts, [&](uint i, Uplifting::Vertex &vert) {
        ImGui::SetNextItemWidth(ImGui::GetWindowWidth() * 0.25);
        if (ImGui::BeginCombo("##constraint_type", std::format("{}", vert.constraint).c_str())) {
          if (ImGui::Selectable("direct", std::holds_alternative<DirectColorConstraint>(vert.constraint))) {
            if (!std::holds_alternative<DirectColorConstraint>(vert.constraint)) {
              vert.constraint = DirectColorConstraint { .colr_i = 0.5 };
            }
          }
          if (ImGui::Selectable("direct surface", std::holds_alternative<DirectSurfaceConstraint>(vert.constraint))) {
            if (!std::holds_alternative<DirectSurfaceConstraint>(vert.constraint)) {
              vert.constraint = DirectSurfaceConstraint();
            }
          }
          if (ImGui::Selectable("indirect surface", std::holds_alternative<IndirectSurfaceConstraint>(vert.constraint))) {
            if (!std::holds_alternative<IndirectSurfaceConstraint>(vert.constraint)) {
              vert.constraint = IndirectSurfaceConstraint();
            }
          }
          if (ImGui::Selectable("measurement", std::holds_alternative<MeasurementConstraint>(vert.constraint))) {
            if (!std::holds_alternative<MeasurementConstraint>(vert.constraint)) {
              vert.constraint = MeasurementConstraint { .measurement = 0.5  };
            }
          }
          ImGui::EndCombo();
        }
      });
      ImGui::SameLine();
      
      // Vertex property view column
      detail::visit_range_column<Uplifting::Vertex>("Properties", 0.4, value.verts, [&](uint i, Uplifting::Vertex &vert) {
        std::visit(overloaded {
          [](DirectColorConstraint &cstr) { },
          [](DirectSurfaceConstraint &cstr) {
            // Show primary color value taken from surface
            auto srgb = (eig::Array4f() << lrgb_to_srgb(cstr.surface.diffuse), 1).finished();
            ImGui::ColorButton("##base_color", srgb, ImGuiColorEditFlags_NoPicker | ImGuiColorEditFlags_Float);

            // Show secondary color constraints, up to a maximum amount before overflow
            for (uint i = 0; i < std::min(3u, static_cast<uint>(cstr.colr_j.size())); ++i) {
              auto srgb = (eig::Array4f() << lrgb_to_srgb(cstr.colr_j[i]), 1).finished();
              ImGui::SameLine();
              ImGui::ColorButton("##base_color", srgb, ImGuiColorEditFlags_NoPicker | ImGuiColorEditFlags_Float);
            }
          },
          [](IndirectSurfaceConstraint &cstr) { },
          [](MeasurementConstraint &cstr) { },
        }, vert.constraint);
      });
      ImGui::SameLine();

      ImGui::BeginGroup();
      {
        ImGui::AlignTextToFramePadding();
        ImGui::NewLine();
        ImGui::SameLine(36.f);
        
        // Optionally insert hide/show button to hide/show all vertices
        bool is_any_active = rng::any_of(value.verts, &Uplifting::Vertex::is_active);
        if (ImGui::Button(is_any_active ? "V" : "H")) {
          is_any_active = !is_any_active;
          rng::fill(value.verts | vws::transform(&Uplifting::Vertex::is_active), is_any_active);
        }
        if (ImGui::IsItemHovered())
          ImGui::SetTooltip("Toggle all active");
        ImGui::SameLine();

        // Button and accompanying popup to add new constraint vertices
        if (ImGui::Button("+")) {
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
        
        for (uint j = 0; j < value.verts.size(); ++j) {
          // Get shorthands
          auto scope = ImGui::ScopedID(std::format("dadada_{}_{}", typeid(Uplifting::Vertex).name(), j));
          auto &vert = value.verts[j];

          // Edit button spawns MMVEditorTask window
          if (ImGui::Button("Edit")) {
            auto child_name   = std::format("mmv_editor_{}_{}", i, j);
            auto child_handle = info.child_task(child_name);
            if (!child_handle.is_init()) {
              child_handle.init<MMVEditorTask>(
                InputSelection { .uplifting_i = i, .constraint_i = j });
            }
          }
          if (ImGui::IsItemHovered())
            ImGui::SetTooltip("Edit constraint");
          ImGui::SameLine();

          // Optionally insert hide/show button at end of line if vertex constraint supports this
          if (ImGui::Button(vert.is_active ? "V" : "H"))
            vert.is_active = !vert.is_active;
          if (ImGui::IsItemHovered())
            ImGui::SetTooltip("Toggle active");
          ImGui::SameLine();

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
