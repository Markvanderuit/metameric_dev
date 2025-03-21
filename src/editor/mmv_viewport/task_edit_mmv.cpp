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

#include <metameric/editor/mmv_viewport/task_edit_mmv.hpp>
#include <metameric/editor/detail/component_edit.hpp>
#include <metameric/editor/detail/file_dialog.hpp>
#include <metameric/core/convex.hpp>
#include <metameric/core/metamer.hpp>
#include <metameric/scene/scene.hpp>
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
    const auto &e_window = info.global("window").getr<gl::Window>();
    const auto &e_scene  = info.global("scene").getr<Scene>();
    const auto &e_cs     = info.parent()("selection").getr<ConstraintRecord>();

    // Obtain the generated spectrum for this uplifting/vertex combination
    const Spec &spec = e_scene.components.upliftings
                              .gl.uplifting_data[e_cs.uplifting_i]
                              .interior[e_cs.vertex_i].spec;

    // Encapsulate editable data, so changes are saved in an undoable manner
    detail::encapsulate_scene_data<ComponentType>(info, e_cs.uplifting_i, [&](auto &info, uint i, ComponentType &uplf) {
      // Return value for coming lambda captures
      enum class PushReturnAction {
        eNone,
        eEdit,
        eDelete
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
        ImGui::Text("%s", e_scene.csys_name(*uplf).c_str());

        // lRGB/sRGB color column
        ImGui::TableSetColumnIndex(2);
        ImGui::ColorEdit3("##lrgb", cstr.colr_i.data(), ImGuiColorEditFlags_NoInputs | ImGuiColorEditFlags_Float);
        ImGui::SameLine();
        if (auto srgb = lrgb_to_srgb(cstr.colr_i); ImGui::ColorEdit3("##srgb", srgb.data(), ImGuiColorEditFlags_NoInputs | ImGuiColorEditFlags_Float))
          cstr.colr_i = srgb_to_lrgb(srgb);
        ImGui::SameLine();
        Colr err = (cstr.colr_i - e_scene.csys(*uplf)(spec)).abs();
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

      // Helper to handle single row for LinearConstraint type
      auto push_colr_cstr_row = [&](std::string                 name,
                                    std::span<LinearConstraint> c_vec, 
                                    uint                        c_j) -> PushReturnAction {
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
      
      // Helper to handle single row for NLinearConstraint type
      auto push_powr_cstr_row = [&](std::string                  name,
                                    std::span<NLinearConstraint> c_vec, 
                                    uint                         c_j) -> PushReturnAction {
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
              ImGui::PlotSpectrum("##output_refl_plot", spec, -.05f, 1.05f, { -1.f, 110.f * e_window.content_scale() });
              ImGui::EndTabItem();
            }

            if (!cstr.cstr_j.empty() && !cstr.cstr_j.back().powr_j.empty() && ImGui::BeginTabItem("Radiance")) {
              // Reconstruct radiance from truncated power series
              Spec s = cstr.cstr_j.back().powr_j[0];
              for (uint i = 0; i < cstr.cstr_j.back().powr_j.size(); ++i)
                s += spec.pow(static_cast<float>(i)) * cstr.cstr_j.back().powr_j[i];
              ImGui::PlotSpectrum("##output_radi_plot", s, -0.05f, s.maxCoeff() + 0.05f, { -1.f, 110.f * e_window.content_scale() });
              ImGui::EndTabItem();
            }

            if (!cstr.cstr_j.empty() && !cstr.cstr_j.back().powr_j.empty() && ImGui::BeginTabItem("Power series")) {
              float s_max = 0.f;
              for (uint i = 0; i < cstr.cstr_j.back().powr_j.size(); ++i)
                s_max = std::max(s_max, cstr.cstr_j.back().powr_j[i].maxCoeff());
              ImGui::PlotSpectra("##output_powr_plot", {}, cstr.cstr_j.back().powr_j, -0.05f, s_max + 0.05f, { -1.f, 128.f * e_window.content_scale() });
              ImGui::EndTabItem();
            }

            ImGui::EndTabBar();
          }
        },
        [&](const auto &cstr) {
          ImGui::SeparatorText("Reflectance");
          ImGui::SameLine();  if (ImGui::SmallButton("Print")) fmt::print("{}\n", spec); // Reenable for printing
          /* ImGui::SameLine();
          if (ImGui::SmallButton("Export")) {
            if (fs::path path; detail::save_dialog(path, "txt"))
              io::save_spec(path, spec);
          } */
          // ImGui::PlotSpectrum("##output_refl_plot", spec, -0.05f, 1.05f, { -1.f, 80.f * e_window.content_scale() });
          // std::vector<Spec>        data = { spec, unpacked_spec };
          // std::vector<std::string> lgnd = { "Exact", "Packed" };
          // ImGui::PlotSpectra("##output_refl_plot", lgnd, data, -.05f, 1.05f, { -1.f, 110.f * e_window.content_scale() });
          ImGui::PlotSpectrum("##output_refl_plot", spec, -.05f, 1.05f, { -1.f, 110.f * e_window.content_scale() });
        }
      };
      
      // Visit the underlying constraint data
      vert.constraint | visit {
        [&](is_linear_constraint auto &cstr) {
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
              auto name = fmt::format("Direct #{}", j);
              auto actn = push_colr_cstr_row(name, cstr.cstr_j, j);
              if (actn == PushReturnAction::eDelete) {
                cstr.cstr_j.erase(cstr.cstr_j.begin() + j);
                if (!cstr.cstr_j.empty())
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
              cstr.cstr_j.push_back(LinearConstraint { });
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

            // Baseline constraint row
            push_base_cstr_row(cstr);

            // Indirect constraint rows
            for (uint j = 0; j < cstr.cstr_j.size(); ++j) {
              auto name = fmt::format("Indirect #{}", j);
              auto actn = push_powr_cstr_row(name, cstr.cstr_j, j);
              if (actn == PushReturnAction::eDelete) {
                cstr.cstr_j.erase(cstr.cstr_j.begin() + j);
                cstr.surfaces.erase(cstr.surfaces.begin() + j);
                if (!cstr.cstr_j.empty())
                  cstr.cstr_j.back().is_active = true;
                break;
              } else if (actn == PushReturnAction::eEdit) {
                std::swap(cstr.cstr_j[j],   cstr.cstr_j.back());
                std::swap(cstr.surfaces[j], cstr.surfaces.back());
                cstr.cstr_j.back().is_active = true;
                break;
              }
            }

            // Add button
            ImGui::TableNextRow();
            ImGui::TableSetColumnIndex(0);
            if (ImGui::Button("Add")) {
              cstr.cstr_j.push_back(NLinearConstraint { });
              cstr.surfaces.push_back(SurfaceInfo::invalid());
            }
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

        // Show optional color patches
        const auto &e_patches = info.relative("viewport_gen_patches")("patches")
                                    .template getr<std::vector<Colr>>();
        if (!e_patches.empty()) {
          // Visual separator from editing components drawn in previous tasks
          ImGui::SeparatorText("Mismatch color samples");
          
          for (uint i = 0; i < e_patches.size(); ++i) {
            // Wrap around if we are out of line space
            if (ImGui::GetContentRegionAvail().x < 32.f)
              ImGui::NewLine();

            // Spawn color button viewing the srgb-transformed patch color
            Colr lrgb = e_patches[i];
            auto srgb = (eig::Array4f() << lrgb_to_srgb(lrgb), 1.f).finished();
            if (ImGui::ColorButton(fmt::format("##patch_{}", i).c_str(), srgb, ImGuiColorEditFlags_Float))
              vert.set_mismatch_position(lrgb);
            
            if (i < e_patches.size() - 1)
              ImGui::SameLine();
          } // for (uint i)
        }
        
        // Visual separator from editing components drawn in previous tasks
        ImGui::SeparatorText("Mismatch color volume");
      }
    });
  }
} // namespace met