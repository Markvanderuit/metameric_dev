// Copyright (C) 2024 Mark van de Ruit, Delft University of Technology.
// 
// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.
// 
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
// 
// You should have received a copy of the GNU General Public License
// along with this program.  If not, see <https://www.gnu.org/licenses/>.

#include <metameric/scene/scene.hpp>
#include <metameric/core/utility.hpp>
#include <metameric/editor/detail/imgui.hpp>
#include <metameric/editor/detail/component_edit.hpp>
#include <metameric/editor/task_mmv_editor.hpp>
#include <metameric/editor/detail/file_dialog.hpp>
#include <concepts>

namespace met {
  namespace detail {
    // Helper function; given a title, access to a set of textures, and a modifiable variant
    // representing a color or a texture, spawn a combo box for texture/color selection
    constexpr
    void push_texture_variant_selector_3f(const std::string &title, const auto &resources, auto &variant) {
      // First, spawn a editor for the variant's specific type; color editor, or texture selector
      ImGui::SetNextItemWidth(ImGui::GetContentRegionAvail().x * 0.75);
      if (std::holds_alternative<Colr>(variant)) {
        auto lrgb = std::get<Colr>(variant);
        ImGui::ColorEdit3(fmt::format("##_{}_lrgb", title).c_str(), lrgb.data()/* , ImGuiColorEditFlags_Float */);
        ImGui::SameLine();
        if (auto srgb = lrgb_to_srgb(lrgb); ImGui::ColorEdit3(fmt::format("##_{}_srgb", title).c_str(), srgb.data(), ImGuiColorEditFlags_NoInputs /* | ImGuiColorEditFlags_Float */))
          lrgb = srgb_to_lrgb(srgb);
        variant = lrgb;
      } else if (std::holds_alternative<uint>(variant)) {
        push_resource_selector(fmt::format("##_{}_txtr", title), resources, std::get<uint>(variant));
      }
      
      // Then, spawn a combobox to switch between the variant's types
      ImGui::SameLine();
      ImGui::SetNextItemWidth(ImGui::GetContentRegionAvail().x);
      std::array<std::string, 2> select_titles = { "Value", "Texture" };
      if (ImGui::BeginCombo(fmt::format("##_{}_data", title).c_str(), title.c_str())) {
        if (ImGui::Selectable("Value", std::holds_alternative<Colr>(variant)))
          variant = Colr(1);
        if (ImGui::Selectable("Texture", std::holds_alternative<uint>(variant)))
          variant = uint(0u);
        ImGui::EndCombo();
      } // If (BeginCombo)
    }

    constexpr
    void push_texture_optional_selector(const std::string &title, const auto &resources, auto &j) {
      auto name = j.transform([&](uint j) { return resources[j].name.c_str(); }) .value_or("None");

      if (ImGui::BeginCombo(title.data(), name)) {
        if (ImGui::Selectable("None", !j))
          j = {};
        for (uint i = 0; i < resources.size(); ++i)
          if (ImGui::Selectable(resources[i].name.c_str(), j && (*j == i)))
            j = i;
        ImGui::EndCombo();
      } // if (BeginCombo)
    }

    // Helper function; given a title, access to a set of textures, and a modifiable variant
    // representing a color or a texture, spawn a combo box for texture/color selection
    constexpr
    void push_texture_variant_selector_1f(
      const std::string &title, 
      const auto &resources, 
      auto &variant,
      float minv = 0.f,
      float maxv = 1.f
    ) {
      // First, spawn a editor for the variant's specific type; color editor, or texture selector
      ImGui::SetNextItemWidth(ImGui::GetContentRegionAvail().x * 0.75);
      if (std::holds_alternative<float>(variant)) {
        auto value = std::get<float>(variant);
        ImGui::SliderFloat(fmt::format("##_{}_value", title).c_str(), &value, minv, maxv);
        ImGui::SameLine();
        variant = value;
      } else if (std::holds_alternative<uint>(variant)) {
        push_resource_selector(fmt::format("##_{}_txtr", title), resources, std::get<uint>(variant));
      }
      
      // Then, spawn a combobox to switch between the variant's types
      ImGui::SameLine();
      ImGui::SetNextItemWidth(ImGui::GetContentRegionAvail().x);
      if (ImGui::BeginCombo(fmt::format("##_{}_data", title).c_str(), title.c_str())) {
        if (ImGui::Selectable("Value", std::holds_alternative<float>(variant)))
          variant = float(0.f);
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

      // We handle scaling on one slider;
      // Important catch; prevent scale from falling to 0, something somewhere breaks :D
      float _scaling = value.transform.scaling.x();
      ImGui::DragFloat("Scaling", &_scaling, 0.01f, 0.001f, 100.f);
      value.transform.scaling = std::max(_scaling, 0.001f);

      ImGui::Separator();

      // Type selector
      if (ImGui::BeginCombo("BRDF Type", fmt::format("{}", value.brdf_type).c_str())) {
        for (uint i = 0; i < 4; ++i) {
          auto type = static_cast<Object::BRDFType>(i);
          auto name = fmt::format("{}", type);
          if (ImGui::Selectable(name.c_str(), value.brdf_type == type)) {
            value.brdf_type = type;
          }
        } // for (uint i)
        ImGui::EndCombo();
      }
        
      // Texture selectors
      if (value.brdf_type != Object::BRDFType::eNull) {
        push_texture_variant_selector_3f("Albedo", scene.resources.images, value.diffuse);
      }
      if (value.brdf_type == Object::BRDFType::eMicrofacet) {
        push_texture_variant_selector_1f("Roughness", scene.resources.images, value.roughness);
        push_texture_variant_selector_1f("Metallic",  scene.resources.images, value.metallic);
        if (ImGui::SliderFloat("Eta", &value.eta_minmax[0], 1.f, 4.f))
          value.eta_minmax[1] = value.eta_minmax[0];
      }
      if (value.brdf_type == Object::BRDFType::eDielectric) {
        push_texture_variant_selector_1f("Roughness", scene.resources.images, value.roughness);
        ImGui::SliderFloat2("Eta (min, max)", value.eta_minmax.data(), 1.f, 4.f);
        ImGui::SliderFloat("Absorption", &value.absorption, 1.f, 100.0f);
      }

      ImGui::Separator();
      
      push_texture_optional_selector("Normalmap", scene.resources.images, value.normalmap);
    };

    // Default implementation of editing visitor for Emitter components
    template <>
    void edit_visitor_default(SchedulerHandle &info, uint i, Component<Emitter> &component) {
      // Get external resources and shorthands
      const auto &scene = info.global("scene").getr<Scene>();
      auto &value = component.value;
      
      // Type selector
      if (ImGui::BeginCombo("Type", fmt::format("{}", value.type).c_str())) {
        for (uint i = 0; i < 4; ++i) {
          auto type = static_cast<Emitter::Type>(i);
          auto name = fmt::format("{}", type);
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

      // Illuminant distribution/uplifted color
      
      // Type selector
      if (ImGui::BeginCombo("Illuminant type", fmt::format("{}", value.spec_type).c_str())) {
        for (uint i = 0; i < 2; ++i) {
          auto type = static_cast<Emitter::SpectrumType>(i);
          auto name = fmt::format("{}", type);
          if (ImGui::Selectable(name.c_str(), value.spec_type == type)) {
            value.spec_type = type;
          }
        } // for (uint i)
        ImGui::EndCombo();
      }

      // Target distribution
      if (value.spec_type == Emitter::SpectrumType::eIllm) {
        push_resource_selector("Spectrum", scene.resources.illuminants, value.illuminant_i);
      }
      if (value.spec_type == Emitter::SpectrumType::eColr) {
        push_texture_variant_selector_3f("Color", scene.resources.images, value.color);
      }
      ImGui::DragFloat("Power", &value.illuminant_scale, 0.005f, 0.0f, 100.f);
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
      push_resource_selector("Base CMFS",       scene.resources.observers,   value.observer_i);
      push_resource_selector("Base illuminant", scene.resources.illuminants, value.illuminant_i);
      push_resource_selector("Basis functions", scene.resources.bases,       value.basis_i);

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
          auto scope = ImGui::ScopedID(fmt::format("table_row_{}", j));

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
            auto combo_str = to_capital(fmt::format("{}", vert.constraint));
            if (ImGui::BeginCombo("##constraint_type", combo_str.c_str())) {
              vert.constraint | visit_types([&](auto default_v, bool holds_alternative) {
                auto selectable_str = to_capital(fmt::format("{}", default_v));
                if (ImGui::Selectable(selectable_str.c_str(), holds_alternative) && !holds_alternative)
                  vert.constraint = default_v;
              });
              ImGui::EndCombo();
            }
          }
          
          // Properties view column
          ImGui::TableSetColumnIndex(2);
          vert.constraint | visit {
            [](is_linear_constraint auto &cstr) {
              // Show primary color value
              auto srgb = (eig::Array4f() << lrgb_to_srgb(cstr.colr_i), 1).finished();
              ImGui::ColorButton("##base_colr", srgb, ImGuiColorEditFlags_NoPicker | ImGuiColorEditFlags_Float);

              // Show secondary color constraints
              for (auto &cstr_j : cstr.cstr_j | vws::take(3ul)) {
                auto srgb = (eig::Array4f() << lrgb_to_srgb(cstr_j.colr_j), 1).finished();
                ImGui::SameLine();
                ImGui::ColorButton("##cstr_colr", srgb, ImGuiColorEditFlags_NoPicker | ImGuiColorEditFlags_Float);
              }
            },
            [](is_nlinear_constraint auto &cstr) {
              // Show primary color value
              auto srgb = (eig::Array4f() << lrgb_to_srgb(cstr.colr_i), 1).finished();
              ImGui::ColorButton("##base_colr", srgb, ImGuiColorEditFlags_NoPicker | ImGuiColorEditFlags_Float);
            
              // Show secondary color constraints
              for (auto &cstr_j : cstr.cstr_j | vws::take(3ul)) {
                auto srgb = (eig::Array4f() << lrgb_to_srgb(cstr_j.colr_j), 1).finished();
                ImGui::SameLine();
                ImGui::ColorButton("##cstr_colr", srgb, ImGuiColorEditFlags_NoPicker | ImGuiColorEditFlags_Float);
              }
            },
            [](auto &) {}
          };

          // Edit buttons
          ImGui::TableSetColumnIndex(3);
          {
            // Edit button spawns MMVEditorTask window
            if (ImGui::Button("Edit")) {
              auto child_name   = fmt::format("mmv_editor_{}_{}", i, j);
              auto child_handle = info.child_task(child_name);
              if (!child_handle.is_init()) {
                child_handle.init<MMVEditorTask>(
                  ConstraintRecord { .uplifting_i = i, .vertex_i = j });
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
              auto child_name   = fmt::format("mmv_editor_{}_{}", i, j);
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
              .constraint = MeasurementConstraint { .measure = 0.5  }
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

    // Default implementation of editing visitor for view components
    template <>
    void edit_visitor_default(SchedulerHandle &info, uint i, Component<View> &component) {
      // Get external resources and shorthands
      const auto &scene = info.global("scene").getr<Scene>();
      auto &value = component.value;

      push_resource_selector("CMFS", scene.resources.observers, value.observer_i);
      ImGui::Checkbox("Draw frustrum",      &value.draw_frustrum);
      ImGui::DragFloat("Field of view (y)", &value.camera_fov_y, 1.f, .05f, 90.f);
      ImGui::InputScalarN("Film size",      ImGuiDataType_U32, value.film_size.data(), 2);
      ImGui::DragFloat3("Position",         value.camera_trf.position.data(), 0.01f, -100.f, 100.f);
      ImGui::DragFloat3("Rotation",         value.camera_trf.rotation.data(), 0.01f, -10.f, 10.f);
    };

    // Default implementation of editing visitor for Mesh resources
    template <>
    void edit_visitor_default(SchedulerHandle &info, uint i, const Resource<Mesh> &resource) {
      // Get external resources and shorthands
      const auto &scene = info.global("scene").getr<Scene>();
      const auto &value = resource.value();

      size_t size_bytes = cnt_span<const std::byte>(value.verts).size()
                        + cnt_span<const std::byte>(value.elems).size();
      
      ImGui::LabelText("Vertices", "%zu", value.verts.size());
      ImGui::LabelText("Elements", "%zu", value.elems.size());
      ImGui::LabelText("Bytes",  "%d", size_bytes);
    };

    // Default implementation of editing visitor for Mesh resources
    template <>
    void edit_visitor_default(SchedulerHandle &info, uint i, const Resource<Image> &resource) {
      // Get external resources and shorthands
      const auto &scene = info.global("scene").getr<Scene>();
      const auto &value = resource.value();

      ImGui::LabelText("Width",  "%d", value.size().x());
      ImGui::LabelText("Height", "%d", value.size().y());
      ImGui::LabelText("Bytes",  "%d", value.data().size());
    };
  } // namespace detail
} // namespace met
