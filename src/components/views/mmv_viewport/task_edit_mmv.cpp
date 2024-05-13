#include <metameric/components/views/mmv_viewport/task_edit_mmv.hpp>
#include <metameric/components/views/detail/component_edit.hpp>
#include <metameric/components/views/detail/file_dialog.hpp>
#include <metameric/core/metamer.hpp>
#include <metameric/core/mesh.hpp>
#include <metameric/core/image.hpp>
#include <metameric/core/matching.hpp>
#include <metameric/core/ranges.hpp>
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
    const auto &e_window  = info.global("window").getr<gl::Window>();
    const auto &e_scene   = info.global("scene").getr<Scene>();
    const auto &e_cs      = info.parent()("selection").getr<ConstraintRecord>();
    const auto &e_uplift  = e_scene.components.upliftings[e_cs.uplifting_i].value;
    const auto &e_basis   = e_scene.resources.bases[e_uplift.basis_i].value();
    auto uplf_handle      = info.task(std::format("gen_upliftings.gen_uplifting_{}", e_cs.uplifting_i)).mask(info);
    const auto &e_spectra = uplf_handle("constraint_spectra").getr<std::vector<Spec>>();
    const auto &e_coeffs  = uplf_handle("constraint_coeffs").getr<std::vector<Basis::vec_type>>();
    const auto &e_hulls   = uplf_handle("mismatch_hulls").getr<std::vector<ConvexHull>>();
    const auto &e_hull    = e_hulls[e_cs.vertex_i];

    // Generate packed-unpacked roundtrip spectrum to compare to full precision distribution
    Spec spec = e_spectra[e_cs.vertex_i], unpacked_spec;
    Basis::vec_type coeffs = e_coeffs[e_cs.vertex_i], unpacked_coeffs;
    if constexpr (wavelength_bases == 12) {
      unpacked_coeffs = detail::unpack_snorm_12(detail::pack_snorm_12(e_coeffs[e_cs.vertex_i]));
      unpacked_spec   = e_basis(coeffs);
    } else if constexpr (wavelength_bases == 16) {
      spec = e_spectra[e_cs.vertex_i];
      unpacked_coeffs = detail::unpack_snorm_16(detail::pack_snorm_16(e_coeffs[e_cs.vertex_i]));
      unpacked_spec   = e_basis(coeffs);
    }  
    
    // Encapsulate editable data, so changes are saved in an undoable manner
    detail::encapsulate_scene_data<ComponentType>(info, e_cs.uplifting_i, [&](auto &info, uint i, ComponentType &uplf) {
      // Return value for coming lambda captures
      enum class PushReturnAction {
        eNone,
        eEdit,
        eDelete
      };

      auto push_separator = [&]() {
        ImGui::TableNextRow();
        ImGui::Spacing();
      };
      
      // Helper to handle single row for types with .colr_i
      auto push_base_cstr_row = [&](auto &cstr) {
        ImGui::TableNextRow();
        auto scope = ImGui::ScopedID("Base");

        // Name column
        ImGui::TableSetColumnIndex(0);
        ImGui::AlignTextToFramePadding();
        ImGui::Text("Base");

        // CSYS editor column
        ImGui::TableSetColumnIndex(1);
        ImGui::SetNextItemWidth(ImGui::GetContentRegionAvail().x);
        ImGui::Text(e_scene.csys_name(uplf.value.csys_i).c_str());

        // lRGB/sRGB color column
        ImGui::TableSetColumnIndex(2);
        ImGui::ColorEdit3("##lrgb", cstr.colr_i.data(), ImGuiColorEditFlags_NoInputs | ImGuiColorEditFlags_Float);
        ImGui::SameLine();
        if (auto srgb = lrgb_to_srgb(cstr.colr_i); ImGui::ColorEdit3("##srgb", srgb.data(), ImGuiColorEditFlags_NoInputs | ImGuiColorEditFlags_Float))
          cstr.colr_i = srgb_to_lrgb(srgb);
        ImGui::SameLine();
        Colr err = (cstr.colr_i - e_scene.csys(uplf.value.csys_i)(spec)).abs();
        ImGui::ColorEdit3("##err", err.data(), ImGuiColorEditFlags_NoInputs | ImGuiColorEditFlags_Float);
        
        // Empty col, not deletable
        ImGui::TableSetColumnIndex(3);
        
        // Is-Active column for forcibly disabling linear part of IndirectSurfaceConstraint
        ImGui::TableSetColumnIndex(4);
        if constexpr (is_roundtrip_constraint<std::decay_t<decltype(cstr)>>) {
          ImGui::Checkbox("##is_base_active", &cstr.is_base_active);
          if (ImGui::IsItemHovered())
            ImGui::SetTooltip("Set base constraint (in)active");
        } else {
          ImGui::BeginDisabled();
          bool null_b = true;
          ImGui::Checkbox("##is_base_active", &null_b);
          ImGui::EndDisabled();
        }
      };

      // Helper to handle single row for ColrConstraint type
      auto push_colr_cstr_row = [&](std::string               name,
                                    std::span<ColrConstraint> c_vec, 
                                    uint                      c_j) -> PushReturnAction {
        ImGui::TableNextRow();
        auto scope = ImGui::ScopedID(name);
        auto &cstr = c_vec[c_j];
        auto  csys = e_scene.csys(cstr.cmfs_j, cstr.illm_j);

        // Return value set to this return value
        PushReturnAction action = PushReturnAction::eNone;

        // Edit column
        ImGui::TableSetColumnIndex(0);
        if (c_j == c_vec.size() - 1) {
          ImGui::BeginDisabled();
          ImGui::Button("Edit");
          ImGui::EndDisabled();
        } else {
          if (ImGui::Button("Edit"))
            action = PushReturnAction::eEdit;
          if (ImGui::IsItemHovered())
            ImGui::SetTooltip("Make mismatching constraint");
        }

        // CSYS editor column
        ImGui::TableSetColumnIndex(1);
        ImGui::SetNextItemWidth(ImGui::GetContentRegionAvail().x * 0.5);
        push_resource_selector("##cmfs", e_scene.resources.observers, cstr.cmfs_j);
        ImGui::SameLine();
        ImGui::SetNextItemWidth(ImGui::GetContentRegionAvail().x);
        push_resource_selector("##illm", e_scene.resources.illuminants, cstr.illm_j);
        
        // lRGB/sRGB/error column
        ImGui::TableSetColumnIndex(2);
        ImGui::ColorEdit3("##lrgb", cstr.colr_j.data(), ImGuiColorEditFlags_NoInputs | ImGuiColorEditFlags_Float);
        ImGui::SameLine();
        if (auto srgb = lrgb_to_srgb(cstr.colr_j); ImGui::ColorEdit3("##srgb", srgb.data(), ImGuiColorEditFlags_NoInputs | ImGuiColorEditFlags_Float))
          cstr.colr_j = srgb_to_lrgb(srgb);
        ImGui::SameLine();
        Colr err = (cstr.colr_j - csys(spec)).abs();
        ImGui::ColorEdit3("##err", err.data(), ImGuiColorEditFlags_NoInputs | ImGuiColorEditFlags_Float);
          
        // Delete/Edit column
        ImGui::TableSetColumnIndex(3);
        if (ImGui::Button("X"))
          action = PushReturnAction::eDelete;
        if (ImGui::IsItemHovered())
          ImGui::SetTooltip("Delete constraint");
          
        // Is-Active column
        ImGui::TableSetColumnIndex(4);
        if (c_j == c_vec.size() - 1)
          ImGui::BeginDisabled();
        ImGui::Checkbox("##is_active", &cstr.is_active);
        if (ImGui::IsItemHovered())
          ImGui::SetTooltip("Set constraint (in)active");
        if (c_j == c_vec.size() - 1)
          ImGui::EndDisabled();

        return action;
      };
      
      // Helper to handle single row for ColrConstraint type
      auto push_powr_cstr_row = [&](std::string               name,
                                    std::span<PowrConstraint> c_vec, 
                                    uint                      c_j) -> PushReturnAction {
        ImGui::TableNextRow();
        auto scope = ImGui::ScopedID(name);
        auto &cstr = c_vec[c_j];
        auto csys = IndirectColrSystem { .cmfs   = e_scene.resources.observers[cstr.cmfs_j].value(), 
                                         .powers = cstr.powr_j };

        // Return value set to this return value
        PushReturnAction action = PushReturnAction::eNone;

        // Edit column
        ImGui::TableSetColumnIndex(0);
        if (c_j == c_vec.size() - 1) {
          ImGui::BeginDisabled();
          ImGui::Button("Edit");
          ImGui::EndDisabled();
        } else {
          if (ImGui::Button("Edit"))
            action = PushReturnAction::eEdit;
          if (ImGui::IsItemHovered())
            ImGui::SetTooltip("Make mismatching constraint");
        }

        // CSYS editor column
        ImGui::TableSetColumnIndex(1);
        ImGui::SetNextItemWidth(ImGui::GetContentRegionAvail().x);
        push_resource_selector("##cmfs", e_scene.resources.observers, cstr.cmfs_j);
  
        // lRGB/sRGB/error column
        ImGui::TableSetColumnIndex(2);
        ImGui::ColorEdit3("##lrgb", cstr.colr_j.data(), ImGuiColorEditFlags_NoInputs | ImGuiColorEditFlags_Float);
        ImGui::SameLine();
        if (auto srgb = lrgb_to_srgb(cstr.colr_j); ImGui::ColorEdit3("##srgb", srgb.data(), ImGuiColorEditFlags_NoInputs | ImGuiColorEditFlags_Float))
          cstr.colr_j = srgb_to_lrgb(srgb);
        ImGui::SameLine();
        Colr err = (cstr.colr_j - csys(spec)).abs();
        ImGui::ColorEdit3("##err", err.data(), ImGuiColorEditFlags_NoInputs | ImGuiColorEditFlags_Float);

        // Delete/Edit column
        ImGui::TableSetColumnIndex(3);
        if (ImGui::Button("X"))
          action = PushReturnAction::eDelete;
        if (ImGui::IsItemHovered())
          ImGui::SetTooltip("Delete constraint");
        
        // Is-Active column
        ImGui::TableSetColumnIndex(4);
        if (c_j == c_vec.size() - 1)
          ImGui::BeginDisabled();
        ImGui::Checkbox("##is_active", &cstr.is_active);
        if (ImGui::IsItemHovered())
          ImGui::SetTooltip("Set constraint (in)active");
        if (c_j == c_vec.size() - 1)
          ImGui::EndDisabled();

        return action;
      };

      // Get modified vertex
      auto &vert = uplf.value.verts[e_cs.vertex_i];
      
      // Plotter for the current constraint's resulting spectrum and
      // several underlying distributions
      vert.constraint | visit {
        [&](const IndirectSurfaceConstraint &cstr) {
          if (ImGui::BeginTabBar("##tab_bar")) {
            if (ImGui::BeginTabItem("Reflectance")) {
              std::vector<Spec>        data = { spec, unpacked_spec };
              std::vector<std::string> lgnd = { "Exact", "Packed"   };
              ImGui::PlotSpectra("##output_refl_plot", lgnd, data, -.05f, 1.05f, { -1.f, 110.f * e_window.content_scale() });
              ImGui::EndTabItem();
            }
            if (ImGui::BeginTabItem("Coefficients")) {
              std::vector<Basis::vec_type> coef = { coeffs, unpacked_coeffs };
              std::vector<std::string> lgnd     = { "Exact", "Packed"       };
              if (ImPlot::BeginPlot("##output_coef_plot", { -1.f, 256.f * e_window.content_scale() }, ImPlotFlags_Crosshairs | ImPlotFlags_NoFrame)) {
                Basis::vec_type x_values;
                rng::copy(vws::iota(0u, wavelength_bases), x_values.begin());
                ImPlot::SetupAxes("Index", "Value", ImPlotAxisFlags_NoGridLines, ImPlotAxisFlags_NoDecorations);
                ImPlot::SetupAxesLimits(-1.f, wavelength_bases, -1.f, 1.f, ImPlotCond_Always);
                for (const auto &[text, vals] : met::vws::zip(lgnd, coef))
                  ImPlot::PlotLine(text.c_str(), x_values.data(), vals.data(), wavelength_bases);
              }
              ImPlot::EndPlot();
            }
            if (!cstr.cstr_j_indrct.empty() && !cstr.cstr_j_indrct.back().powr_j.empty() && ImGui::BeginTabItem("Radiance")) {
              // Reconstruct radiance from truncated power series
              Spec s = cstr.cstr_j_indrct.back().powr_j[0];
              for (uint i = 0; i < cstr.cstr_j_indrct.back().powr_j.size(); ++i)
                s += spec.pow(static_cast<float>(i)) * cstr.cstr_j_indrct.back().powr_j[i];
              ImGui::PlotSpectrum("##output_radi_plot", s, -0.05f, s.maxCoeff() + 0.05f, { -1.f, 110.f * e_window.content_scale() });
              ImGui::EndTabItem();
            }
            if (!cstr.cstr_j_indrct.empty() && !cstr.cstr_j_indrct.back().powr_j.empty() && ImGui::BeginTabItem("Power series")) {
              float s_max = 0.f;
              for (uint i = 0; i < cstr.cstr_j_indrct.back().powr_j.size(); ++i)
                s_max = std::max(s_max, cstr.cstr_j_indrct.back().powr_j[i].maxCoeff());
              ImGui::PlotSpectra("##output_powr_plot", {}, cstr.cstr_j_indrct.back().powr_j, -0.05f, s_max + 0.05f, { -1.f, 128.f * e_window.content_scale() });
              ImGui::EndTabItem();
            }

            ImGui::EndTabBar();
          }
        },
        [&](const auto &cstr) {
          ImGui::SeparatorText("Reflectance");
          /* ImGui::SameLine();  if (ImGui::SmallButton("Print")) fmt::print("{}\n", spec); */ // Reenable for printing
          /* ImGui::SameLine();
          if (ImGui::SmallButton("Export")) {
            if (fs::path path; detail::save_dialog(path, "txt"))
              io::save_spec(path, spec);
          } */
          // ImGui::PlotSpectrum("##output_refl_plot", spec, -0.05f, 1.05f, { -1.f, 80.f * e_window.content_scale() });
          std::vector<Spec>        data = { spec, unpacked_spec };
          std::vector<std::string> lgnd = { "Exact", "Packed" };
          ImGui::PlotSpectra("##output_refl_plot", lgnd, data, -.05f, 1.05f, { -1.f, 110.f * e_window.content_scale() });
        }
      };
      
      // Visit the underlying constraint data
      vert.constraint | visit {
        [&](is_colr_constraint auto &cstr) {
          ImGui::SeparatorText("Constraints");
          if (ImGui::BeginTable("##table", 5, ImGuiTableFlags_SizingStretchProp)) {
            // Setup table header; columns are shown without hover or color; cleaner than table headers
            ImGui::TableSetupScrollFreeze(0, 1);
            ImGui::TableNextRow();
            ImGui::TableSetColumnIndex(1); ImGui::Text("Color system");
            ImGui::TableSetColumnIndex(2); ImGui::Text("lrgb/srgb/error");

            // Baseline constraint row
            push_base_cstr_row(cstr);

            // Direct constraint rows
            for (uint j = 0; j < cstr.cstr_j.size(); ++j) {
              auto name = std::format("Direct #{}", j);
              auto actn = push_colr_cstr_row(name, cstr.cstr_j, j);
              if (actn == PushReturnAction::eDelete) {
                cstr.cstr_j.erase(cstr.cstr_j.begin() + j);
                cstr.cstr_j.back().is_active = true;
                break;
              } else if (actn == PushReturnAction::eEdit) {
                std::swap(cstr.cstr_j[j], cstr.cstr_j.back());
                cstr.cstr_j.back().is_active = true;
                break;
              }
            }

            // Add button
            ImGui::TableNextRow();
            ImGui::TableSetColumnIndex(0);
            if (ImGui::Button("Add"))
              cstr.cstr_j.push_back(ColrConstraint { });
            if (ImGui::IsItemHovered())
              ImGui::SetTooltip("Add new constraint");
            
            ImGui::EndTable();
          }
        },
        [&](IndirectSurfaceConstraint &cstr) {
          ImGui::SeparatorText("Constraints");
          if (ImGui::BeginTable("##table", 5, ImGuiTableFlags_SizingStretchProp)) {
            // Setup table header; columns are shown without hover or color; cleaner than table headers
            ImGui::TableSetupScrollFreeze(0, 1);
            ImGui::TableNextRow();
            ImGui::TableSetColumnIndex(1); ImGui::Text("Color system");
            ImGui::TableSetColumnIndex(2); ImGui::Text("lrgb/srgb/error");

            // Checkbox for all
            ImGui::TableSetColumnIndex(4);
            if (!cstr.cstr_j_indrct.empty()) {
              // auto is_active_v = cstr.cstr_j | vws::transform(&ColrConstraint::is_active);
              // auto is_active   = is_active_v | rng::to<std::vector<int>>();
              // ImGui::CheckboxFlags("##is_active", is_active.data(), is_active.size());
              // rng::copy(is_active, is_active_v.begin());
            }

            // Baseline constraint row
            push_base_cstr_row(cstr);

            // Direct constraint rows
            for (uint j = 0; j < cstr.cstr_j_direct.size(); ++j) {
              auto name = std::format("Direct #{}", j);
              auto actn = push_colr_cstr_row(name, cstr.cstr_j_direct, j);
              if (actn == PushReturnAction::eDelete) {
                cstr.cstr_j_direct.erase(cstr.cstr_j_direct.begin() + j);
                cstr.cstr_j_direct.back().is_active = true;
                break;
              } else if (actn == PushReturnAction::eEdit) {
                std::swap(cstr.cstr_j_direct[j], cstr.cstr_j_direct.back());
                cstr.cstr_j_direct.back().is_active = true;
                break;
              }
            }

            // Indirect constraint rows
            for (uint j = 0; j < cstr.cstr_j_indrct.size(); ++j) {
              auto name = std::format("Indirect #{}", j);
              auto actn = push_powr_cstr_row(name, cstr.cstr_j_indrct, j);
              if (actn == PushReturnAction::eDelete) {
                cstr.cstr_j_indrct.erase(cstr.cstr_j_indrct.begin() + j);
                cstr.cstr_j_indrct.back().is_active = true;
                break;
              } else if (actn == PushReturnAction::eEdit) {
                std::swap(cstr.cstr_j_indrct[j], cstr.cstr_j_indrct.back());
                cstr.cstr_j_indrct.back().is_active = true;
                break;
              }
            }

            // Add button
            ImGui::TableNextRow();
            ImGui::TableSetColumnIndex(0);
            if (ImGui::Button("Add"))
              cstr.cstr_j_indrct.push_back(PowrConstraint { });
            if (ImGui::IsItemHovered())
              ImGui::SetTooltip("Add new constraint");
            
            ImGui::EndTable();
          }
        },
        [&](MeasurementConstraint &cstr) {
          ImGui::Separator();
          if (ImGui::Button("Import from file")) {
            if (fs::path path; detail::load_dialog(path)) {
              cstr.measure = io::load_spec(path);
            }
          }
        }
      };

      // Last parts before mismatch volume editor is spawned
      if (vert.has_mismatching(e_scene, uplf.value)) {
        // Visual separator from editing components drawn in previous tasks
        ImGui::SeparatorText("Mismatching");

        
        if (ImGui::SmallButton("Save image")) {
          if (fs::path path; detail::save_dialog(path, "exr")) {
            // Get shared texture resource
            const auto &e_txtr = info.relative("viewport_image")("lrgb_target").getr<gl::Texture2d4f>();
            debug::check_expr(e_txtr.is_init());

            // Create cpu-side rgba image matching gpu-side film format
            Image image = {{
              .pixel_frmt = Image::PixelFormat::eRGBA,
              .pixel_type = Image::PixelType::eFloat,
              .size       = e_txtr.size()
            }};

            // Copy over to cpu-side
            e_txtr.get(cast_span<float>(image.data()));

            // Save to exr; no gamma correction
            image.save_exr(path);
          }
        }
        ImGui::SameLine();
        if (ImGui::SmallButton("Print hull")) {
          fmt::print("verts\n{}\n\n", e_hull.hull.verts);
          fmt::print("elems\n{}\n\n", e_hull.hull.elems);
        }

        /* // Toggle button for clipping
        auto &e_clip = info.relative("viewport_guizmo")("clip_point").getw<bool>();
        ImGui::Checkbox("Clip to volume", &e_clip);
        if (ImGui::IsItemHovered())
          ImGui::SetTooltip("Metamers can only be correctly generated inside the volume"); */

        // Show optional color patches
        const auto &e_patches = info.relative("viewport_gen_patches")("patches").getr<std::vector<Colr>>();
        if (!e_patches.empty()) {
          for (uint i = 0; i < e_patches.size(); ++i) {
            // Wrap around if we are out of line space
            if (ImGui::GetContentRegionAvail().x < 32.f)
              ImGui::NewLine();

            // Spawn color button viewing the srgb-transformed patch color
            Colr lrgb = e_patches[i];
            auto srgb = (eig::Array4f() << lrgb_to_srgb(lrgb), 1.f).finished();
            if (ImGui::ColorButton(std::format("##patch_{}", i).c_str(), srgb, ImGuiColorEditFlags_Float))
              vert.set_mismatch_position(lrgb);
            
            if (i < e_patches.size() - 1)
              ImGui::SameLine();
          } // for (uint i)
        }
      }
    });
  }
} // namespace met