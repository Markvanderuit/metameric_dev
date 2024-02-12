#include <metameric/core/scene.hpp>
#include <metameric/core/utility.hpp>
#include <metameric/components/views/detail/imgui.hpp>
#include <metameric/components/views/detail/component_edit.hpp>
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
    void edit_visitor_default(SchedulerHandle &info, Component<Object> &component) {
      // Get external resources and shorthands
      const auto &e_scene = info.global("scene").getr<Scene>();
      auto &value = component.value;

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
    template <>
    void edit_visitor_default(SchedulerHandle &info, Component<Emitter> &component) {
      // Get external resources and shorthands
      const auto &e_scene = info.global("scene").getr<Scene>();
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
      push_resource_selector("Illuminant", e_scene.resources.illuminants, value.illuminant_i);
      ImGui::DragFloat("Power", &value.illuminant_scale, 0.1f, 0.0f, 100.f);
    };

    // Default implementation of editing visitor for Uplifting components
    template <>
    void edit_visitor_default(SchedulerHandle &info, Component<Uplifting> &component) {
      // Get external resources and shorthands
      const auto &e_scene = info.global("scene").getr<Scene>();
      auto &value = component.value;
      
      // Uplifting value modifications
      push_resource_selector("Basis functions", e_scene.resources.bases, value.basis_i);
      push_resource_selector("Base color system", e_scene.components.colr_systems, value.csys_i);

      // Per-constraint vertex
      for (uint i = 0; i < value.verts.size(); ++i) {
        // Get shorthands
        auto &vert  = value.verts[i];
        auto _scope = ImGui::ScopedID(std::format("{}_{}", typeid(Uplifting::Vertex).name(), i));
        auto _name  = std::format("Constraint {} ({})", i, vert.constraint);
        
        // Add treenode section; postpone jumping into section
        bool section_open = vert.has_surface() && ImGui::TreeNodeEx(_name.c_str());
        
        // Insert delete button, is_active button on same line
        if constexpr (has_active_value<Uplifting::Vertex>) {
          ImGui::SameLine(ImGui::GetContentRegionMax().x - 38.f);
          if (ImGui::SmallButton(vert.is_active ? "V" : "H"))
            vert.is_active = !vert.is_active;
        }
        ImGui::SameLine(ImGui::GetContentRegionMax().x - 16.f);
        if (ImGui::SmallButton("X")) {
          value.verts.erase(value.verts.begin() + i);
          break;
        }

        // Continue if treenode is not open
        guard_continue(section_open);

        // Visitor handles the four different constraint types
        std::visit(overloaded {
          [&](DirectSurfaceConstraint &c) {
            guard(c.is_valid());
            
            ImGui::InputFloat3("Surface position", c.surface.p.data());
            ImGui::InputFloat3("Surface normal",   c.surface.n.data());
            ImGui::ColorEdit3("Surface diffuse",   c.surface.diffuse.data(),
                  ImGuiColorEditFlags_Float | ImGuiColorEditFlags_NoOptions);
          },
          [](auto &) {
            ImGui::Text("Not implemented");
          }
        }, vert.constraint);
        
        // Close treenode if the visitor got this far
        ImGui::TreePop();
      } // for (uint i)
    };

    // Default implementation of editing visitor for ColorSystem components
    template <>
    void edit_visitor_default(SchedulerHandle &info, Component<ColorSystem> &component) {
      // Get external resources and shorthands
      const auto &e_scene = info.global("scene").getr<Scene>();
      auto &value = component.value;
      
      push_resource_selector("CMFS", e_scene.resources.observers, value.observer_i);
      push_resource_selector("Illuminant", e_scene.resources.illuminants, value.illuminant_i);

      // Force update name to adhere to [CMFS][Illuminant] naming clarity
      component.name = e_scene.get_csys_name(value);
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
} // namespace met
