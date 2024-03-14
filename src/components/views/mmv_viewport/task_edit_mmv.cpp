#include <metameric/components/views/mmv_viewport/task_edit_mmv.hpp>
#include <metameric/components/views/detail/component_edit.hpp>
#include <metameric/core/metamer.hpp>
#include <metameric/core/ranges.hpp>
#include <small_gl/texture.hpp>
#include <small_gl/window.hpp>
#include <implot.h>

namespace met {
  // TODO remove
  namespace detail {
    // Helper method to spawn a imgui group of a fixed width, and then call a visitor()
    // over every element of a range, s.t. a column is built for all elements
    template <typename Ty>
    void visit_range_column(const std::string               &col_name,
                            float                            col_width,
                            rng::sized_range auto           &range,
                            std::function<void (uint, Ty &)> visitor) {
      ImGui::BeginGroup();
      ImGui::AlignTextToFramePadding();

      if (col_name.empty()) {
        ImGui::NewLine();
      } else {
        ImGui::SetNextItemWidth(col_width);
        ImGui::Text(col_name.c_str());
      }

      for (uint j = 0; j < range.size(); ++j) {
        auto scope = ImGui::ScopedID(std::format("{}", j));
        visitor(j, range[j]);
      }

      ImGui::EndGroup();
    }
  } // namespace detail


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
          // Color baseline editor
          {
            // lRGB color picker
            Colr &lrgb = cstr.get_colr_i();
            ImGui::ColorEdit3("Base color (lrgb)", cstr.get_colr_i().data(), ImGuiColorEditFlags_Float);
            
            // sRGB color picker
            Colr srgb = lrgb_to_srgb(lrgb);
            ImGui::ColorEdit3("Base color (srgb)", srgb.data(), ImGuiColorEditFlags_Float);
            lrgb = srgb_to_lrgb(srgb);
            
            // Roundtrip error
            Colr rtrp = e_scene.get_csys(0).apply(e_spectra[e_is.constraint_i]);
            Colr err  = (lrgb - rtrp).abs();
            ImGui::InputFloat3("Roundtrip (lrgb)", err.data(), "%.3f", ImGuiInputTextFlags_ReadOnly);
            // ImGui::ColorEdit3("Roundtrip (lrgb)", err.data(), ImGuiColorEditFlags_Float);
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
            auto systems  = { colsys_i.finalize() };
            auto signals  = { cstr.colr_i };

            // Generate spectral distribution
            Spec s = generate_spectrum(GenerateSpectrumInfo {
              .basis   = e_scene.resources.bases[uplf.value.basis_i].value(),
              .systems = systems,
              .signals = signals
            });
            
            // Assign the reset accompanying color
            cstr.colr_j[i] = colsys_j.apply(s);
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
            ImGui::ColorEdit3("Surface color (lrgb)", cstr.get_colr_i().data(), ImGuiColorEditFlags_Float);
            auto srgb = lrgb_to_srgb(cstr.get_colr_i());
            ImGui::ColorEdit3("Surface color (srgb)", srgb.data(), ImGuiColorEditFlags_Float);
            cstr.get_colr_i() = srgb_to_lrgb(srgb);
          }

          // Visual separator into constraint list
          ImGui::Separator();

          auto cstr_srgb = lrgb_to_srgb(cstr.colr);
          ImGui::ColorEdit3("Constraint radiance (lrgb)", cstr.colr.data(), ImGuiColorEditFlags_Float);
          ImGui::ColorEdit3("Constraint radiance (srgb)", cstr_srgb.data(), ImGuiColorEditFlags_Float);

          // Visual separator into constraint list
          // ImGui::Separator();

          /* // Get wavelength values for x-axis in plot
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
          } */

          ImGui::SeparatorText("Estimated output");
          {
            // Reconstruct radiance from truncated power series
            Spec r = e_spectra[e_is.constraint_i];
            Spec s = cstr.powers[0];
            for (uint i = 1; i < cstr.powers.size(); ++i)
              s += r.pow(static_cast<float>(i)) * cstr.powers[i];

            // Recover output color
            IndirectColrSystem csys = {
              .cmfs   = e_scene.resources.observers[e_scene.components.observer_i.value].value(),
              .powers = cstr.powers
            };
            Colr colr_lrgb = csys(r);
            Colr colr_srgb = lrgb_to_srgb(colr_lrgb);

            // Plot color
            ImGui::ColorEdit3("Roundtrip radiance (lrgb)", colr_lrgb.data(), ImGuiColorEditFlags_Float);
            ImGui::ColorEdit3("Roundtrip radiance (srgb)", colr_srgb.data(), ImGuiColorEditFlags_Float);

            Colr err = colr_lrgb - cstr.colr;
            ImGui::ColorEdit3("Roundtrip error (lrgb)", err.data(), ImGuiColorEditFlags_Float);

            ImGui::SeparatorText("Radiance spectrum");

            // Plot radiance
            if (ImPlot::BeginPlot("##output_radi_plot", { -1.f, 128.f * e_window.content_scale() }, ImPlotFlags_NoInputs | ImPlotFlags_NoFrame)) {
              // Get wavelength values for x-axis in plot
              Spec x_values;
              rng::copy(vws::iota(0u, wavelength_samples) | vws::transform(wavelength_at_index), x_values.begin());

              // Setup minimal format for coming line plots
              ImPlot::SetupLegend(ImPlotLocation_North, ImPlotLegendFlags_Horizontal | ImPlotLegendFlags_Outside);
              ImPlot::SetupAxes("Wavelength", "##Value", ImPlotAxisFlags_NoGridLines, ImPlotAxisFlags_NoDecorations);
              ImPlot::SetupAxesLimits(wavelength_min, wavelength_max, -0.05f, s.maxCoeff() + 0.05f, ImPlotCond_Always);

              // Do the thing
              ImPlot::PlotLine("", x_values.data(), s.data(), wavelength_samples);
              ImPlot::EndPlot();
            }
          }
        },
        [&](MeasurementConstraint &cstr) {
          ImGui::Text("Not implemented");
        }
      }, vert.constraint);
    });

    // Plotter for the current constraint's resulting spectrum
    ImGui::SeparatorText("Reflectance spectrum");
    {
      // Get shared resources
      const auto &e_window  = info.global("window").getr<gl::Window>();
      const auto &e_sd      = e_spectra[e_is.constraint_i];

      if (ImPlot::BeginPlot("##output_refl_plot", { -1.f, 128.f * e_window.content_scale() }, ImPlotFlags_NoInputs | ImPlotFlags_NoFrame)) {
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
    }
  }
} // namespace met