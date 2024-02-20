#include <metameric/components/views/mmv_viewport/task_edit_mmv.hpp>
#include <metameric/components/views/detail/component_edit.hpp>
#include <metameric/core/metamer.hpp>
#include <small_gl/texture.hpp>

namespace met {
  bool EditMMVTask::is_active(SchedulerHandle &info) {
    met_trace();
    return info.parent()("is_active").getr<bool>();
  }

  void EditMMVTask::eval(SchedulerHandle &info) {
    met_trace();

    using ComponentType = detail::Component<Uplifting>;

    // Get shared resources
    const auto &e_is = info.parent()("selection").getr<InputSelection>();
    
    // Encapsulate editable data, so changes are saved in an undoable manner
    detail::encapsulate_scene_data<ComponentType>(info, e_is.uplifting_i, [&](auto &info, uint i, ComponentType &uplf) {
      auto &vert = uplf.value.verts[e_is.constraint_i];
      const auto &e_scene = info.global("scene").getr<Scene>();
      
      // Visit the underlying constraint data
      std::visit(overloaded {
        [&](DirectColorConstraint &cstr) {
          // Color baseline value
          {
            // ImGui::ColorEdit3("Base color", cstr.colr_i.data(), ImGuiColorEditFlags_Float);
            auto srgb = lrgb_to_srgb(cstr.colr_i);
            ImGui::ColorEdit3("Base color", srgb.data(), ImGuiColorEditFlags_Float);
            cstr.colr_i = srgb_to_lrgb(srgb);
          }

          // Visual separator into constraint list
          ImGui::Separator(); 

          // Maximum column width is part of available content region
          float col_width = ImGui::GetContentRegionAvail().x;

          // Color constraint; system column
          detail::visit_range_column<uint>("Color system", col_width * 0.35, cstr.csys_j, [&](uint i, uint &csys_j) {
            // Spawn selector; work on a copy to detect changes
            uint _csys_j = csys_j;
            detail::push_resource_selector<detail::Component<ColorSystem>>("##selector", e_scene.components.colr_systems, _csys_j, 
              [](const auto &c) { return c.name; });

            // On a change, we reset the accompanying color value to a valid center
            // by doing a quick spectral roundtrip with a known valid metamer
            guard(csys_j != _csys_j);
            csys_j = _csys_j;

            // Gather relevant color systems
            auto colsys_i = e_scene.get_csys(uplf.value.csys_i);
            auto colsys_j = e_scene.get_csys(csys_j);
            auto systems  = { colsys_i.finalize_direct() };
            auto signals  = { cstr.colr_i };

            // Generate spectral distribution
            Spec s = generate_spectrum({
              .basis   = e_scene.resources.bases[uplf.value.basis_i].value(),
              .systems = systems,
              .signals = signals
            });
            
            // Assign the reset accompanying color
            cstr.colr_j[i] = colsys_j.apply_color_direct(s);
          });
          ImGui::SameLine();

          // Color constraint; value column
          detail::visit_range_column<Colr>("Color value", col_width * 0.35, cstr.colr_j, [&](uint i, Colr &colr_j) {
            // ImGui::ColorEdit3("##color_editor", colr_j.data(), ImGuiColorEditFlags_Float | ImGuiColorEditFlags_NoInputs);
            auto srgb = lrgb_to_srgb(colr_j);
            ImGui::ColorEdit3("##color_editor", srgb.data(), ImGuiColorEditFlags_Float | ImGuiColorEditFlags_NoInputs);
            colr_j = srgb_to_lrgb(srgb);
          });
          
          if (ImGui::Button("Add constraint")) {
            cstr.csys_j.push_back(0);
            cstr.colr_j.push_back(cstr.colr_i);
          }
        },
        [&](DirectSurfaceConstraint &cstr) {
          // Color baseline value extracted from surface
          ImGui::ColorEdit3("Base color", cstr.surface.diffuse.data(), ImGuiColorEditFlags_Float);
          
          // Visual separator into constraint list
          ImGui::Separator();

          // Maximum column width is part of available content region
          float col_width = ImGui::GetContentRegionAvail().x;
          
          // Color constraint; system column
          detail::visit_range_column<uint>("Color system", col_width * 0.35, cstr.csys_j, [&](uint i, uint &csys_j) {
            detail::push_resource_selector<detail::Component<ColorSystem>>("##selector", e_scene.components.colr_systems, csys_j, 
              [](const auto &c) { return c.name; });
          });
          ImGui::SameLine();

          // Color constraint; value column
          detail::visit_range_column<Colr>("Color value", col_width * 0.35, cstr.colr_j, [&](uint i, Colr &colr_j) {
            ImGui::ColorEdit3("##color_editor", colr_j.data(), ImGuiColorEditFlags_Float | ImGuiColorEditFlags_NoInputs);
          });
          
          if (ImGui::Button("Add constraint")) {
            cstr.csys_j.push_back(0);
            cstr.colr_j.push_back(cstr.surface.diffuse);
          }
        },
        [&](IndirectSurfaceConstraint &cstr) {
          ImGui::Text("Not implemented");
        },
        [&](MeasurementConstraint &cstr) {
          ImGui::Text("Not implemented");
        }
      }, vert.constraint);
    });

    
  }
} // namespace met