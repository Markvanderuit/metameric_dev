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

      // Visual separator for vertex constraints
      // which are layed out in a table
      ImGui::SeparatorText("Constraints");
      if (!value.verts.empty() && ImGui::BeginTable("Properties", 4, ImGuiTableFlags_SizingStretchProp)) {
        // Setup table header; column 4 is left without header
        // Columns are shown without hover or color; cleaner than using header
        ImGui::TableSetupScrollFreeze(0, 1);
        ImGui::TableNextRow();
        ImGui::TableSetColumnIndex(0); ImGui::Text("Name");
        ImGui::TableSetColumnIndex(1); ImGui::Text("Type");
        ImGui::TableSetColumnIndex(2); ImGui::Text("Data");
        
        // Each next row is dedicated to a single vertex constraint
        for (uint j = 0; j < value.verts.size(); ++j) {
          ImGui::TableNextRow();
          auto &vert = value.verts[j];
          auto scope = ImGui::ScopedID(std::format("table_row_{}", j));

          // Name editor column
          ImGui::TableSetColumnIndex(0);
          ImGui::SetNextItemWidth(ImGui::GetContentRegionAvail().x);
          auto copy = vert.name; 
          if (ImGui::InputText("##constraint_name", &copy, str_edit_flags)) {
            vert.name = copy;
          }

          // Type editor column
          ImGui::TableSetColumnIndex(1);
          ImGui::SetNextItemWidth(ImGui::GetContentRegionAvail().x);
          {
            // Iterate over the types in the std::variant of constraints
            // for this combobox
            auto combo_str = to_capital(std::format("{}", vert.constraint));
            if (ImGui::BeginCombo("##constraint_type", combo_str.c_str())) {
              variant_visit<Uplifting::Vertex::cnstr_type>([&](auto v) {
                auto selectable_str = to_capital(std::format("{}", v));
                bool holds_ty       = std::holds_alternative<decltype(v)>(vert.constraint);
                if (ImGui::Selectable(selectable_str.c_str(), holds_ty) && !holds_ty)
                  vert.constraint = v;
              });
              ImGui::EndCombo();
            }
          }
          
          // Properties view column
          ImGui::TableSetColumnIndex(2);
          std::visit(overloaded {
            [](ColorConstraint auto &cstr) {
              // Show primary color value
              auto srgb = (eig::Array4f() << lrgb_to_srgb(cstr.get_colr_i()), 1).finished();
              ImGui::ColorButton("##base_colr", srgb, ImGuiColorEditFlags_NoPicker | ImGuiColorEditFlags_Float);

              // Show secondary color constraints
              for (auto &colr_j : cstr.colr_j | vws::take(3ul)) {
                auto srgb = (eig::Array4f() << lrgb_to_srgb(colr_j), 1).finished();
                ImGui::SameLine();
                ImGui::ColorButton("##cstr_colr", srgb, ImGuiColorEditFlags_NoPicker | ImGuiColorEditFlags_Float);
              }
            },
            [](auto &cstr) { },
          }, vert.constraint);

          // Edit buttons
          ImGui::TableSetColumnIndex(3);
          {
            // Edit button spawns MMVEditorTask window
            if (ImGui::Button("Edit")) {
              auto child_name   = std::format("mmv_editor_{}_{}", i, j);
              auto child_handle = info.child_task(child_name);
              if (!child_handle.is_init()) {
                child_handle.init<MMVEditorTask>(
                  ConstraintSelection { .uplifting_i = i, .constraint_i = j });
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
              // Despawn MMVEditorTask window if necessary
              auto child_name   = std::format("mmv_editor_{}_{}", i, j);
              auto child_handle = info.child_task(child_name);
              if (child_handle.is_init()) {
                child_handle.mask(info)("is_active").set(false);
                child_handle.dstr();
              }

              // Remove vertex from data
              value.verts.erase(value.verts.begin() + j);

              // Exit early
              break;
            }
            if (ImGui::IsItemHovered())
              ImGui::SetTooltip("Delete component");
          }
        } // for (uint j)
        ImGui::EndTable();
      } 

      // Add button and accompanying popup to add new constraint vertices
      {
        if (ImGui::Button("New constraint")) {
          ImGui::OpenPopup("popup_add_uplifting_vertex");
        }
        if (ImGui::BeginPopup("popup_add_uplifting_vertex")) {
          if (ImGui::Selectable("Direct"))
            value.verts.push_back({ 
              .name       = "Direct Color",
              .constraint = DirectColorConstraint { .colr_i = 0.5  }
            });
          if (ImGui::Selectable("Measurement"))
            value.verts.push_back({ 
              .name       = "Meaurement",
              .constraint = MeasurementConstraint { .measurement = 0.5  }
            });
          if (ImGui::Selectable("Direct surface"))
            value.verts.push_back({ 
              .name       = "Direct Surface",
              .constraint = DirectSurfaceConstraint()
            });
          if (ImGui::Selectable("Indirect surface"))
            value.verts.push_back({ 
              .name       = "Indirect Surface",
              .constraint = IndirectSurfaceConstraint()
            });
          ImGui::EndPopup();
        } // if (BeginPopup())
      }
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
      component.name = scene.csys_name(value);
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
