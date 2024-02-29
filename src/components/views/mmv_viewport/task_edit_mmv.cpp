#include <metameric/components/views/mmv_viewport/task_edit_mmv.hpp>
#include <metameric/components/views/detail/component_edit.hpp>
#include <metameric/core/metamer.hpp>
#include <small_gl/texture.hpp>
#include <small_gl/window.hpp>
#include <implot.h>

namespace met {
  bool EditMMVTask::is_active(SchedulerHandle &info) {
    met_trace();
    return info.parent()("is_active").getr<bool>();
  }

  void EditMMVTask::eval(SchedulerHandle &info) {
    met_trace();

    using ComponentType = detail::Component<Uplifting>;

    // Get shared resources
    const auto &e_scene   = info.global("scene").getr<Scene>();
    const auto &e_is      = info.parent()("selection").getr<InputSelection>();
    auto uplf_handle      = info.task(std::format("gen_upliftings.gen_uplifting_{}", e_is.uplifting_i)).mask(info);
    const auto &e_spectra = uplf_handle("constraint_spectra").getr<std::vector<Spec>>();
    
    // Encapsulate editable data, so changes are saved in an undoable manner
    detail::encapsulate_scene_data<ComponentType>(info, e_is.uplifting_i, [&](auto &info, uint i, ComponentType &uplf) {
      auto &vert            = uplf.value.verts[e_is.constraint_i];
      const auto &e_window  = info.global("window").getr<gl::Window>();
      
      // Visit the underlying constraint data
      std::visit(overloaded {
        [&](DirectColorConstraint &cstr) {
          // Color baseline value
          {
            ImGui::ColorEdit3("Base color (lrgb)", cstr.get_colr_i().data(), ImGuiColorEditFlags_Float);
            auto srgb = lrgb_to_srgb(cstr.get_colr_i());
            ImGui::ColorEdit3("Base color (srgb)", srgb.data(), ImGuiColorEditFlags_Float);
            cstr.get_colr_i() = srgb_to_lrgb(srgb);
          }

          // Visual separator into constraint list
          ImGui::Separator(); 

          // Maximum column width is part of available content region
          float col_width = ImGui::GetContentRegionAvail().x;

          // Color constraint; system column
          detail::visit_range_column<uint>("Color system", col_width * 0.35, cstr.csys_j, [&](uint i, uint &csys_j) {
            const auto &e_scene = info.global("scene").getr<Scene>();

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
          {
            ImGui::ColorEdit3("Base color (lrgb)", cstr.get_colr_i().data(), ImGuiColorEditFlags_Float);
            auto srgb = lrgb_to_srgb(cstr.get_colr_i());
            ImGui::ColorEdit3("Base color (srgb)", srgb.data(), ImGuiColorEditFlags_Float);
            cstr.get_colr_i() = srgb_to_lrgb(srgb);
          }
          
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
          // Color baseline value extracted from surface
          {
            ImGui::ColorEdit3("Base color (lrgb)", cstr.get_colr_i().data(), ImGuiColorEditFlags_Float);
            auto srgb = lrgb_to_srgb(cstr.get_colr_i());
            ImGui::ColorEdit3("Base color (srgb)", srgb.data(), ImGuiColorEditFlags_Float);
            cstr.get_colr_i() = srgb_to_lrgb(srgb);
          }

          // Visual separator into constraint list
          ImGui::Separator();

          // Get wavelength values for x-axis in plot
          Spec x_values;
          rng::copy(vws::iota(0u, wavelength_samples) | vws::transform(wavelength_at_index), x_values.begin());

          uint p = 0;
          for (const auto &spec : cstr.powers) {
            // Run a spectrum plot for the accumulated radiance
            if (ImPlot::BeginPlot(std::format("power {}", p).c_str(),
                                  { 256.f * e_window.content_scale(), 96.f * e_window.content_scale() }, ImPlotFlags_NoInputs | ImPlotFlags_NoFrame)) {
              ImPlot::SetupLegend(ImPlotLocation_North, ImPlotLegendFlags_Horizontal | ImPlotLegendFlags_Outside);
              ImPlot::SetupAxesLimits(wavelength_min, wavelength_max, spec.minCoeff(), spec.maxCoeff(), ImPlotCond_Always);
              ImPlot::PlotLine("##rad", x_values.data(), spec.data(), wavelength_samples);
              ImPlot::EndPlot();
            }
            p++;
          }

          ImGui::SeparatorText("Estimated output");
          {
            // Reconstruct radiance from truncated power series
            Spec r = e_spectra[e_is.constraint_i];
            Spec s = 0.f;
            for (uint i = 0; i < cstr.powers.size(); ++i)
              s += r.pow(static_cast<float>(i)) * cstr.powers[i];

            // Get camera cmfs
            CMFS cmfs = e_scene.resources.observers[e_scene.components.observer_i.value].value();
            cmfs = (cmfs.array())
                 / (cmfs.array().col(1) * wavelength_ssize).sum();
            cmfs = (models::xyz_to_srgb_transform * cmfs.matrix().transpose()).transpose();

            // Recover output color
            Colr colr_lrgb = (cmfs.transpose() * s.matrix());
            Colr colr_srgb = lrgb_to_srgb(colr_lrgb);

            // Plot color
            ImGui::ColorEdit3("Radiance color", colr_srgb.data(), ImGuiColorEditFlags_Float);

            // Plot radiance
            if (ImPlot::BeginPlot("Radiance distr", { -1.f, 128.f * e_window.content_scale() }, ImPlotFlags_NoInputs | ImPlotFlags_NoFrame)) {
              // Get wavelength values for x-axis in plot
              Spec x_values;
              rng::copy(vws::iota(0u, wavelength_samples) | vws::transform(wavelength_at_index), x_values.begin());

              // Simple barebones spectrum plot
              ImPlot::SetupLegend(ImPlotLocation_North, ImPlotLegendFlags_Horizontal | ImPlotLegendFlags_Outside);
              ImPlot::SetupAxesLimits(wavelength_min, wavelength_max, -0.05f, s.maxCoeff() + 0.05f, ImPlotCond_Always);
              ImPlot::PlotLine("##radiance_line", x_values.data(), s.data(), wavelength_samples);
              ImPlot::EndPlot();
            }
          }


          ImGui::Text("Not implemented");
        },
        [&](MeasurementConstraint &cstr) {
          ImGui::Text("Not implemented");
        }
      }, vert.constraint);
    });

    // Plotter for the current constraint's resulting spectrum
    ImGui::SeparatorText("Output spectrum");
    {
      // Get shared resources
      const auto &e_window  = info.global("window").getr<gl::Window>();
      const auto &e_sd      = e_spectra[e_is.constraint_i];

      if (ImPlot::BeginPlot("##output_spectrum_plot", { -1.f, 128.f * e_window.content_scale() }, ImPlotFlags_NoInputs | ImPlotFlags_NoFrame)) {
        // Get wavelength values for x-axis in plot
        Spec x_values;
        rng::copy(vws::iota(0u, wavelength_samples) | vws::transform(wavelength_at_index), x_values.begin());
      
        // Setup minimal format for coming line plots
        ImPlot::SetupLegend(ImPlotLocation_North, ImPlotLegendFlags_Horizontal | ImPlotLegendFlags_Outside);
        ImPlot::SetupAxes("Wavelength", "##Value", ImPlotAxisFlags_NoGridLines, ImPlotAxisFlags_NoDecorations);
        ImPlot::SetupAxesLimits(wavelength_min, wavelength_max, -0.05, 1.05, ImPlotCond_Always);

        // Do the thing
        ImPlot::PlotLine("", x_values.data(), e_sd.data(), wavelength_samples);
        ImPlot::EndPlot();
      }

      const auto &e_scene = info.global("scene").getr<Scene>();
      
      Spec illuminant = e_scene.get_emitter_spd(0);
      ColrSystem csys = {
        .cmfs       = e_scene.resources.observers[0].value(),
        .illuminant = e_scene.resources.illuminants[0].value(),
        .n_scatters = 1
      };
      auto colr = csys.apply_color_direct(e_sd);
      ImGui::ColorEdit3("Roundtrip color", colr.data(), ImGuiColorEditFlags_Float);
    }
  }
} // namespace met