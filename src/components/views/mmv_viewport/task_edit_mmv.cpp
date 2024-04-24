#include <metameric/components/views/mmv_viewport/task_edit_mmv.hpp>
#include <metameric/components/views/detail/component_edit.hpp>
#include <metameric/core/metamer.hpp>
#include <metameric/core/matching.hpp>
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
    const auto &e_window  = info.global("window").getr<gl::Window>();
    const auto &e_scene   = info.global("scene").getr<Scene>();
    const auto &e_cs      = info.parent()("selection").getr<ConstraintRecord>();
    auto uplf_handle      = info.task(std::format("gen_upliftings.gen_uplifting_{}", e_cs.uplifting_i)).mask(info);
    const auto &e_spectra = uplf_handle("constraint_spectra").getr<std::vector<Spec>>();
    const auto &e_coeffs  = uplf_handle("constraint_coeffs").getr<std::vector<Basis::vec_type>>();
    const auto &e_patches = info.relative("viewport_gen_patches")("patches").getr<std::vector<Colr>>();
    
    // Encapsulate editable data, so changes are saved in an undoable manner
    detail::encapsulate_scene_data<ComponentType>(info, e_cs.uplifting_i, [&](auto &info, uint i, ComponentType &uplf) {
      // Get modified vertex
      auto &vert = uplf.value.verts[e_cs.vertex_i];

      if (ImGui::Button("Print range")) {
        Spec phase;
        rng::copy(vws::iota(0u, wavelength_samples)| vws::transform(wavelength_at_index), phase.begin());
        fmt::print("{}\n", phase);
      }
      
      // Plotter for the current constraint's resulting spectrum
      ImGui::SeparatorText("Reflectance spectrum");
      {
        const auto &e_sd = e_spectra[e_cs.vertex_i];
        ImGui::SameLine();
        if (ImGui::SmallButton("Print"))
          fmt::print("{}\n", e_sd);
        ImGui::PlotSpectrum("##output_refl_plot", e_sd, -0.05f, 1.05f, { -1.f, 96.f * e_window.content_scale() });
      }

      // Plotter for the current constraint's resulting radiance
      // (only for IndirectSurfaceConstraint, really)
      vert.constraint | visit_single([&](IndirectSurfaceConstraint &cstr) {
        guard(!cstr.powers.empty());

        // Reconstruct radiance from truncated power series
        Spec r = e_spectra[e_cs.vertex_i];
        Spec s = cstr.powers[0];
        for (uint i = 1; i < cstr.powers.size(); ++i)
          s += r.pow(static_cast<float>(i)) * cstr.powers[i];

        // Plot estimated radiance
        ImGui::SeparatorText("Radiance spectrum");
        ImGui::PlotSpectrum("##output_radi_plot", s, -0.05f, s.maxCoeff() + 0.05f, { -1.f, 96.f * e_window.content_scale() });
      });
      
      // Visit the underlying constraint data
      vert.constraint | visit {
        [&](is_colr_constraint auto &cstr) {
          auto baseline_spr_name = std::format("Baseline ({})", e_scene.csys_name(uplf.value.csys_i));
          ImGui::SeparatorText(baseline_spr_name.c_str());
          {
            // lRGB color picker
            Colr &lrgb = cstr.colr_i;
            ImGui::ColorEdit3("Base color (lrgb)", lrgb.data(), ImGuiColorEditFlags_Float);
          
            // sRGB color picker
            Colr srgb = lrgb_to_srgb(lrgb);
            if (ImGui::ColorEdit3("Base color (srgb)", srgb.data(), ImGuiColorEditFlags_Float));
              lrgb = srgb_to_lrgb(srgb);

            // Roundtrip error
            Colr err = (lrgb - e_scene.csys(0)(e_spectra[e_cs.vertex_i])).abs();
            ImGui::InputFloat3("Roundtrip (lrgb)", err.data(), "%.3f", ImGuiInputTextFlags_ReadOnly);
          }

          ImGui::SeparatorText("Constraints");
          {
            if (!cstr.cstr_j.empty() && ImGui::BeginTable("##table", 4, ImGuiTableFlags_SizingStretchProp)) {
              // Setup table header; columns are shown without hover or color; cleaner than table headers
              ImGui::TableSetupScrollFreeze(0, 1);
              ImGui::TableNextRow();
              ImGui::TableSetColumnIndex(0); ImGui::Text("Observer");
              ImGui::TableSetColumnIndex(1); ImGui::Text("Illuminant");
              ImGui::TableSetColumnIndex(2); ImGui::Text("Value (lrgb/srgb/err)");
              
              // Each next row is dedicated to a single color constraint
              for (uint j = 0; j < cstr.cstr_j.size(); ++j) {
                ImGui::TableNextRow();
                auto scope = ImGui::ScopedID(std::format("table_row_{}", j));
                auto &c    = cstr.cstr_j[j];
                
                // CMFS editor column
                ImGui::TableSetColumnIndex(0);
                ImGui::SetNextItemWidth(ImGui::GetContentRegionAvail().x);
                push_resource_selector("##cmfs", e_scene.resources.observers, c.cmfs_j);

                // Illuminant editor column
                ImGui::TableSetColumnIndex(1);
                ImGui::SetNextItemWidth(ImGui::GetContentRegionAvail().x);
                push_resource_selector("##illm", e_scene.resources.illuminants, c.illm_j);

                // lRGB/sRGB color column
                ImGui::TableSetColumnIndex(2);
                ImGui::ColorEdit3("##lrgb", c.colr_j.data(), ImGuiColorEditFlags_NoInputs | ImGuiColorEditFlags_Float);
                ImGui::SameLine();
                if (auto srgb = lrgb_to_srgb(c.colr_j); ImGui::ColorEdit3("##srgb", srgb.data(), ImGuiColorEditFlags_NoInputs | ImGuiColorEditFlags_Float))
                  c.colr_j = srgb_to_lrgb(srgb);
                ImGui::SameLine();
                Colr err = (c.colr_j - e_scene.csys(c.cmfs_j, c.illm_j).apply(e_spectra[e_cs.vertex_i])).abs();
                ImGui::ColorEdit3("##err", err.data(), ImGuiColorEditFlags_NoInputs | ImGuiColorEditFlags_Float);

                // Delete button; exists early to prevent iterator issues
                ImGui::TableSetColumnIndex(3);
                if (ImGui::Button("Edit")) {
                  // TODO set active MMV
                }
                ImGui::SameLine();
                if (ImGui::Button("X")) {
                  // TODO reset active MMV to 0
                  cstr.cstr_j.erase(cstr.cstr_j.begin() + j);
                  break;
                }
                if (ImGui::IsItemHovered())
                  ImGui::SetTooltip("Delete constraint");
              } // for (uint j)

              ImGui::EndTable();
            }

            if (ImGui::Button("New constraint")) {
              cstr.cstr_j.push_back(ColrConstraint { .cmfs_j = 0, .illm_j = 0, .colr_j = 0.f });
            }
          }
        },
        [&](IndirectSurfaceConstraint &cstr) {
          if (!cstr.has_surface() || cstr.powers.empty()) {
            ImGui::Text("Invalid constraint");
            return;
          }
          
          auto baseline_spr_name = std::format("Base constraint ({})", e_scene.resources.observers[e_scene.components.observer_i.value].name);
          ImGui::SeparatorText(baseline_spr_name.c_str());
          {
            {
              auto csys = e_scene.csys(0);
              
              // lRGB color picker
              Colr &lrgb = cstr.surface.diffuse;
              ImGui::ColorEdit3("Surface (lrgb)", lrgb.data(), ImGuiColorEditFlags_Float);

              // sRGB color picker
              Colr srgb = lrgb_to_srgb(lrgb);
              if (ImGui::ColorEdit3("Surface (srgb)", srgb.data(), ImGuiColorEditFlags_Float));
                lrgb = srgb_to_lrgb(srgb);
                
              // Roundtrip error
              Colr err = (lrgb - csys(e_spectra[e_cs.vertex_i])).abs();
              ImGui::InputFloat3("Surface (error)", err.data(), "%.3f", ImGuiInputTextFlags_ReadOnly);
            }
            {
              IndirectColrSystem csys = {
                .cmfs   = e_scene.resources.observers[e_scene.components.observer_i.value].value(),
                .powers = cstr.powers
              };

              // lRGB color picker
              Colr &lrgb = cstr.colr;
              ImGui::ColorEdit3("Constraint (lrgb)", lrgb.data(), ImGuiColorEditFlags_Float);

              // sRGB color picker
              Colr srgb = lrgb_to_srgb(lrgb);
              if (ImGui::ColorEdit3("Constraint (srgb)", srgb.data(), ImGuiColorEditFlags_Float));
                lrgb = srgb_to_lrgb(srgb);
                
              // Roundtrip error
              Colr err = (lrgb - csys(e_spectra[e_cs.vertex_i])).abs();
              ImGui::InputFloat3("Roundtrip (lrgb)", err.data(), "%.3f", ImGuiInputTextFlags_ReadOnly);
            }
          }

          /* {
            auto max_coeff = rng::fold_left_first(
              cstr.powers | vws::transform([](const auto &s) -> float { return s.maxCoeff(); }),
              std::plus<float>()).value();

            ImGui::PlotSpectra("##powers_plot", { }, cstr.powers, -0.05f, max_coeff + 0.05f);
          } */
        },
        [&](MeasurementConstraint &cstr) {
          ImGui::Text("Not implemented");
        }
      };

      // Color patch picker
      if (!e_patches.empty()) {
        ImGui::SeparatorText("Example colors");
        ImGui::BeginChild("##patches", { ImGui::GetContentRegionAvail().x * 0.95f, 64.f * e_window.content_scale() }, 0, ImGuiWindowFlags_HorizontalScrollbar);
        for (uint i = 0; i < e_patches.size(); ++i) {
          // Spawn color button viewing the srgb-transformed patch color
          auto srgb = (eig::Array4f() << lrgb_to_srgb(e_patches[i]), 1).finished();
          if (ImGui::ColorButton(std::format("##patch_{}", i).c_str(), srgb, ImGuiColorEditFlags_Float)) {
            // ...
          }

          // Skip line every now and then
          ImGui::SameLine();
          if (i % 16 == 15 || i == e_patches.size() - 1)
            ImGui::NewLine();
        } // for (uint i)
        ImGui::EndChild();
      }
    });
  }
} // namespace met