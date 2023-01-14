#include <metameric/core/spectrum.hpp>
#include <metameric/core/data.hpp>
#include <metameric/core/io.hpp>
#include <metameric/core/utility.hpp>
#include <metameric/core/detail/trace.hpp>
#include <metameric/components/views/task_spectra_editor.hpp>
#include <metameric/components/views/detail/imgui.hpp>
#include <metameric/components/views/detail/file_dialog.hpp>
#include <small_gl/window.hpp>
#include <implot.h>

namespace met {
  constexpr float plot_height      = 96.f;
  constexpr auto leaf_flags        = ImGuiTreeNodeFlags_Leaf | ImGuiTreeNodeFlags_Bullet | ImGuiTreeNodeFlags_SpanFullWidth;
  constexpr auto plot_flags        = ImPlotFlags_NoFrame | ImPlotFlags_NoMenus;
  constexpr auto plot_y_axis_flags = ImPlotAxisFlags_NoDecorations;
  constexpr auto plot_x_axis_flags = ImPlotAxisFlags_NoLabel | ImPlotAxisFlags_NoGridLines;

  SpectraEditorTask::SpectraEditorTask(const std::string &name)
  : detail::AbstractTask(name) { }

  void SpectraEditorTask::eval(detail::TaskEvalInfo &info) {
    met_trace_full();

    if (ImGui::Begin("Spectra editor")) {
      ImPlot::PushStyleVar(ImPlotStyleVar_FillAlpha, 0.25f);

      // Get shared resources
      auto &e_window      = info.get_resource<gl::Window>(global_key, "window");
      auto &e_appl_data   = info.get_resource<ApplicationData>(global_key, "app_data");
      auto &e_proj_data   = e_appl_data.project_data;
      auto &e_illuminants = e_proj_data.illuminants;
      auto &e_cmfs        = e_proj_data.cmfs;
      auto &e_csys        = e_proj_data.color_systems;

      // Get wavelength values for x-axis in plots
      Spec x_values;
      for (uint i = 0; i < x_values.size(); ++i)
        x_values[i] = wavelength_at_index(i);

      if (ImGui::CollapsingHeader("Data", ImGuiTreeNodeFlags_DefaultOpen)) {
        if (ImGui::TreeNodeEx("Illuminants", ImGuiTreeNodeFlags_DefaultOpen)) {
          for (uint i = 0; i < e_illuminants.size(); ++i) {
            ImGui::PushID(fmt::format("illuminant_data_{}", i).c_str());

            // Get illuminant data
            auto &[key, illuminant] = e_illuminants[i];
                                  
            // Draw bulleted leaf node, wrapped in group for hover detection;
            // inside sits a button to delete the relevant spectrum
            ImGui::BeginGroup();
            ImGui::Bullet();
            ImGui::Text(key.c_str());
            ImGui::SameLine(ImGui::GetContentRegionAvail().x - 96.f);
            if (fs::path path; ImGui::SmallButton("Export") && detail::save_dialog(path, ".spd")) {
              io::save_spec(path, illuminant);
            }
            ImGui::SameLine();
            if (ImGui::SmallButton("X")) e_appl_data.touch({ 
              .name = "Deleted illuminant",
              .redo = [i = i](auto &data) { 
                data.illuminants.erase(data.illuminants.begin() + i);
                for (auto &[_, illm] : data.color_systems) {
                  if (illm > 0 && illm >= i) illm--;
                }
              },
              .undo = [illm = e_illuminants, csys = e_csys](auto &data) { 
                data.illuminants = illm;
                data.color_systems = csys;
              }
            });
            ImGui::EndGroup();
            
            // Plot spectral data on tooltip hover over group
            if (ImGui::IsItemHovered()) {
              ImGui::BeginTooltip();
              if (ImPlot::BeginPlot(key.c_str(), { 0., plot_height * e_window.content_scale() }, plot_flags)) {
                ImPlot::SetupAxes("Wavelength", "Value", plot_x_axis_flags, plot_y_axis_flags);
                ImPlot::PlotLine("##plot_line", x_values.data(), illuminant.data(), wavelength_samples);
                ImPlot::PlotShaded("##plot_line", x_values.data(), illuminant.data(), wavelength_samples);
                ImPlot::EndPlot();
              }
              ImGui::EndTooltip();
            }

            ImGui::PopID();
          } // for (uint i)
          ImGui::TreePop();
        }
        
        if (ImGui::TreeNodeEx("Color matching functions", ImGuiTreeNodeFlags_DefaultOpen)) {
          for (uint i = 0; i < e_cmfs.size(); ++i) {
            ImGui::PushID(fmt::format("cmfs_data_{}", i).c_str());
            
            // Get cmfs column data separately, as it is stored row-major
            auto &[key, cmfs] = e_cmfs[i];
            Spec cmfs_x = cmfs.col(0), cmfs_y = cmfs.col(1), cmfs_z = cmfs.col(2);
            
            // Draw bulleted leaf node, wrapped in group for hover detection;
            // inside sits a button to delete the relevant spectrum
            ImGui::BeginGroup();
            ImGui::Bullet();
            ImGui::Text(key.c_str());
            ImGui::SameLine(ImGui::GetContentRegionAvail().x - 96.f);
            if (fs::path path; ImGui::SmallButton("Export") && detail::save_dialog(path, ".cmfs")) {
              CMFS _cmfs = (models::srgb_to_xyz_transform * cmfs.transpose()).transpose();
              io::save_cmfs(path, _cmfs);
            }
            ImGui::SameLine();
            if (ImGui::SmallButton("X")) e_appl_data.touch({ 
              .name = "Deleted cmfs",
              .redo = [i = i](auto &data) { 
                data.cmfs.erase(data.cmfs.begin() + i);
                for (auto &[cmfs, _] : data.color_systems) {
                  if (cmfs > 0 && cmfs >= i) cmfs--;
                }
              },
              .undo = [cmfs = e_cmfs, csys = e_csys](auto &data) { 
                data.cmfs = cmfs;
                data.color_systems = csys;
              }
            });
            ImGui::EndGroup();

            // Plot spectral data on tooltip hover over group
            if (ImGui::IsItemHovered()) {
              ImGui::BeginTooltip();
              if (ImPlot::BeginPlot(key.c_str(), { 0., plot_height * e_window.content_scale() }, plot_flags)) {
                ImPlot::SetupAxes("Wavelength", "Value", plot_x_axis_flags, plot_y_axis_flags);
                ImPlot::PlotLine("x", x_values.data(), cmfs_x.data(), wavelength_samples);
                ImPlot::PlotLine("y", x_values.data(), cmfs_y.data(), wavelength_samples);
                ImPlot::PlotLine("z", x_values.data(), cmfs_z.data(), wavelength_samples);
                ImPlot::PlotShaded("x", x_values.data(), cmfs_x.data(), wavelength_samples);
                ImPlot::PlotShaded("y", x_values.data(), cmfs_y.data(), wavelength_samples);
                ImPlot::PlotShaded("z", x_values.data(), cmfs_z.data(), wavelength_samples);
                ImPlot::EndPlot();
              }
              ImGui::EndTooltip();
            }

            ImGui::PopID();
          } // for (uint i)
          ImGui::TreePop();
        }

        ImGui::Separator();
        if (fs::path path; ImGui::Button("Add illuminant") && detail::load_dialog(path)) {
          Spec spec = io::load_spec(path);
          auto name = path.filename().replace_extension().string();
          e_illuminants.push_back({ name, spec });
        }
        ImGui::SameLine();
        if (fs::path path; ImGui::Button("Add cmfs") && detail::load_dialog(path)) {
          CMFS cmfs = io::load_cmfs(path);
          auto name = path.filename().replace_extension().string();
          e_cmfs.push_back({ name, cmfs });
        }
      }
      
      if (ImGui::CollapsingHeader("Color systems", ImGuiTreeNodeFlags_DefaultOpen)) {
        ImGui::AlignTextToFramePadding();

        // Coming column widths
        const float total_width = ImGui::GetContentRegionAvail().x;
        const float selec_width  = .45f * total_width;
        const float delet_width  = .05f * total_width;

        // CMFS selectors
        ImGui::BeginGroup(); ImGui::PushItemWidth(selec_width); ImGui::Text("CMFS"); ImGui::Separator();
        for (uint i = 0; i < e_csys.size(); ++i) {
          ImGui::PushID(fmt::format("cmfs_selector_{}", i).c_str());
          
          auto &[cmfs_i, _] = e_csys[i];
          
          if (ImGui::BeginCombo("##CMFS", e_cmfs[cmfs_i].first.c_str())) {
            for (uint j = 0; j < e_cmfs.size(); ++j)
              if (ImGui::Selectable(e_cmfs[j].first.c_str(), cmfs_i == j)) 
                e_appl_data.touch({ 
                  .name = "Modify color system", 
                  .redo = [i = i, j = j](auto &data) { data.color_systems[i].cmfs = j; },
                  .undo = [i = i, j = cmfs_i](auto &data) {  data.color_systems[i].cmfs = j; 
                }});
            ImGui::EndCombo();
          }

          ImGui::PopID();
        }
        ImGui::EndGroup(); ImGui::PopItemWidth(); ImGui::SameLine();

        // Illuminant selectors
        ImGui::BeginGroup(); ImGui::PushItemWidth(selec_width); ImGui::Text("Illuminant"); ImGui::Separator();
        for (uint i = 0; i < e_csys.size(); ++i) {
          ImGui::PushID(fmt::format("illm_selector_{}", i).c_str());

          auto &[_, illum_i] = e_csys[i];

          if (ImGui::BeginCombo("##Illuminant", e_illuminants[illum_i].first.c_str())) {
            for (uint j = 0; j < e_illuminants.size(); ++j)
              if (ImGui::Selectable(e_illuminants[j].first.c_str(), illum_i == j)) 
                e_appl_data.touch({ 
                  .name = "Modify color system", 
                  .redo = [i = i, j = j](auto &data) { data.color_systems[i].illuminant = j; },
                  .undo = [i = i, j = illum_i](auto &data) {  data.color_systems[i].illuminant = j; 
                }});
            ImGui::EndCombo();
          }

          ImGui::PopID();
        }
        ImGui::EndGroup(); ImGui::PopItemWidth(); ImGui::SameLine();

        // Delete buttons
        ImGui::BeginGroup(); ImGui::PushItemWidth(delet_width); ImGui::Text(""); ImGui::Separator();
        for (uint i = 0; i < e_csys.size(); ++i) {
          ImGui::PushID(fmt::format("csys_delete_{}", i).c_str());
          if (ImGui::Button("X")) {
            e_appl_data.touch({ .name = "Delete color system", .redo = [i = i](auto &data) {
              data.color_systems.erase(data.color_systems.begin() + i);
              for (auto &vert : data.gamut_verts)
                for (uint &j : vert.csys_j) if (j >= i) 
                  j--;
            }, .undo = [edit = e_csys, verts = e_proj_data.gamut_verts](auto &data) { 
              data.color_systems = edit;
              data.gamut_verts = verts;
            }});
          }
          ImGui::PopID();
        }
        ImGui::EndGroup(); ImGui::PopItemWidth();

        // Add button
        ImGui::Separator();
        if (ImGui::Button("Add color system")) {
          e_appl_data.touch({
            .name = "Add color system",
            .redo = [](auto &data) { data.color_systems.push_back({ 0, 0 }); },
            .undo = [](auto &data) { data.color_systems.pop_back(); }
          });
        }
      }
      
      ImPlot::PopStyleVar();
    }
    ImGui::End();
  }
} // namespace met