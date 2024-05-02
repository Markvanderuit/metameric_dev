#pragma once

#include <metameric/core/scheduler.hpp>
#include <metameric/core/scene.hpp>
#include <metameric/core/moments.hpp>
#include <metameric/components/views/detail/imgui.hpp>
#include <metameric/components/views/detail/file_dialog.hpp>
#include <metameric/components/pipeline_new/task_gen_uplifting_data.hpp>
#include <small_gl/window.hpp>
#include <format>

namespace met {
  class SceneResourcesEditorTask : public detail::TaskNode {
  public:
    void eval(SchedulerHandle &info) override {
      met_trace_full();

      // Get external resources
      const auto &e_scene  = info.global("scene").getr<Scene>();
      const auto &e_window = info.global("window").getr<gl::Window>();

      // Spawn view of texture atlas interiors
      if (ImGui::Begin("Texture atlas")) {
        const auto &e_atlas = e_scene.resources.images.gl.texture_atlas_3f;
        // Spawn views
        for (uint i = 0; i < e_atlas.capacity().z(); ++i) {
          for (uint j = 0; j < e_atlas.levels(); ++j) {
            ImGui::Image(ImGui::to_ptr(e_atlas.view(i, j).object()), { 256, 256 });
            if (j < e_atlas.levels() - 1)
              ImGui::SameLine();
          }
        }
      }
      ImGui::End();

      // Spawn view of weight atlas interiors
      if (ImGui::Begin("Barycentrics atlas")) {
        const auto &e_txtr = e_scene.components.upliftings.gl.texture_barycentrics;
        const auto &e_coef = e_scene.components.upliftings.gl.texture_coefficients;
        
        // Spawn image over texture view
        const auto &e_view = e_txtr.view(0);
        ImGui::Image(ImGui::to_ptr(e_view.object()), { 4096, 4096 }, { 0, 0 }, { 1, 1 }, ImVec4(1, 1, 1, 1), ImVec4(1, 1, 1, 1));

        // Compute sample position in texture dependent on mouse position in image
        eig::Array2f mouse_pos =(static_cast<eig::Array2f>(ImGui::GetMousePos()) 
                               - static_cast<eig::Array2f>(ImGui::GetItemRectMin()))
                               / static_cast<eig::Array2f>(ImGui::GetItemRectSize());
        auto tooltip_pos = (mouse_pos * e_coef.texture().size().head<2>().cast<float>()).cast<uint>().eval();
             tooltip_pos = tooltip_pos.cwiseMin(e_coef.texture().size().head<2>() - 1);
        const size_t pos = e_coef.texture().size().x() * tooltip_pos.y() + tooltip_pos.x();

        // Spawn tooltip on m-over
        if (ImGui::IsItemHovered() && ImGui::BeginTooltip()) {
          // Hard-stop copy image data to cpu
          eig::Array4u coef_data;
          e_coef.texture().get(cnt_span<uint>(coef_data), 0u, 
                               eig::Array3u { 1, 1, 1 }, 
                               eig::Array3u { tooltip_pos.x(), tooltip_pos.y(), 0 });
          eig::Array4f bary_data;
          e_txtr.texture().get(cnt_span<float>(bary_data), 0u, 
                               eig::Array3u { 1, 1, 1 }, 
                               eig::Array3u { tooltip_pos.x(), tooltip_pos.y(), 0 });
          
          // Unpack barycentric data
          uint        index = static_cast<uint>(bary_data.w());
          eig::Array4f bary = { bary_data.x(), bary_data.y(), bary_data.z(), 1.f - bary_data.head<3>().sum() };

          // Obtain tetrahedron data
          const auto &e_uplf_task = info.task("gen_upliftings.gen_uplifting_0").realize<GenUpliftingDataTask>();
          auto tetr = e_uplf_task.query_tetrahedron(index);

          // Intro info
          ImGui::SetNextItemWidth(256.f * e_window.content_scale());
          ImGui::Text("Inspecting pixel (%i, %i)", tooltip_pos.x(), tooltip_pos.y());

          ImGui::Separator();

          // Recover and plot spectrum
          Basis::vec_type coeffs;
          if constexpr (wavelength_bases == 12) {
            coeffs = detail::unpack_snorm_12(coef_data);
          } else if constexpr (wavelength_bases == 16) {
            coeffs = detail::unpack_snorm_16(coef_data);
          } else {
            // ...
          }
          auto interp_spectrum =(tetr.spectra[0] * bary[0]
                               + tetr.spectra[1] * bary[1]
                               + tetr.spectra[2] * bary[2]
                               + tetr.spectra[3] * bary[3]).eval();
          if constexpr (wavelength_bases == decltype(coeffs)::RowsAtCompileTime) {
            auto coeffs_spectrum = e_scene.resources.bases[0].value()(coeffs);

            // Plot the mixed spectra
            {
              std::vector<std::string> legend = { "a", "b", "c", "d" };
              ImGui::PlotSpectra("Tetrahedron", legend, tetr.spectra, -0.05, 1.05, { 0.f, 128.f });
            }

            ImGui::Separator();

            // Plot the reconstructed spectra
            {
              std::vector<std::string> legend = { "Baseline", "Basis" };
              std::vector<Spec>        spectra = { interp_spectrum, coeffs_spectrum };
              ImGui::PlotSpectra("Reconstruction", legend, spectra, -0.05, 1.05, { 0.f, 128.f });
            }
          }

          ImGui::Separator();

          // Print some minima/maxima
          {
            ImGui::LabelText("Min coeff", "%.3f", interp_spectrum.minCoeff());
            ImGui::LabelText("Max coeff", "%.3f", interp_spectrum.maxCoeff());
            ImGui::InputFloat4("Barycentrics", bary.data(), "%.3f", ImGuiInputTextFlags_ReadOnly);
            ImGui::InputFloat4("Coeffs (r1)", coeffs.data(),     "%.3f", ImGuiInputTextFlags_ReadOnly);
            ImGui::InputFloat4("Coeffs (r2)", coeffs.data() + 4, "%.3f", ImGuiInputTextFlags_ReadOnly);
            ImGui::InputFloat4("Coeffs (r3)", coeffs.data() + 8, "%.3f", ImGuiInputTextFlags_ReadOnly);
          }

          ImGui::EndTooltip();
        }
      }
      ImGui::End();

      return; // TODO implement below

      if (ImGui::Begin("Scene resources")) {
        // Get external resources
        const auto &e_scene = info.global("scene").getr<Scene>();

        if (ImGui::CollapsingHeader(std::format("Meshes ({})", e_scene.resources.meshes.size()).c_str())) {
          for (const auto &mesh : e_scene.resources.meshes) {
            if (ImGui::TreeNodeEx(mesh.name.c_str(), ImGuiTreeNodeFlags_Leaf)) {
              if (ImGui::IsItemHovered()) {
                ImGui::BeginTooltip();
                ImGui::Value("Vertices", static_cast<uint>(mesh.value().verts.size()));
                ImGui::Value("Elements", static_cast<uint>(mesh.value().elems.size()));
                ImGui::EndTooltip();
              }

              ImGui::SameLine(ImGui::GetContentRegionMax().x - 16.f);

              if (ImGui::SmallButton("X")) {
                debug::check_expr(false, "Not implemented");
              }
              
              ImGui::TreePop();
            }
          }
        }

        if (ImGui::CollapsingHeader(std::format("Images ({})", e_scene.resources.images.size()).c_str())) {
          for (uint i = 0; i < e_scene.resources.images.size(); ++i) {
            const auto &image = e_scene.resources.images[i];
            if (ImGui::TreeNodeEx(image.name.c_str(), ImGuiTreeNodeFlags_Leaf)) {
              if (ImGui::IsItemHovered()) {
                ImGui::BeginTooltip();

                ImGui::Text("Dimensions: %i x %i", image.value().size()[0], image.value().size()[1]);
                ImGui::Value("Channels", image.value().channels());
                
                /* if (e_txtr_data.info.size() > i) {
                  const auto &info = e_txtr_data.info[i];
                  const auto *txtr = info.is_3f ? (gl::AbstractTexture*) &e_txtr_data.views_3f[info.layer]
                                                : (gl::AbstractTexture*) &e_txtr_data.views_1f[info.layer];
                  ImGui::Image(ImGui::to_ptr(txtr->object()), { 128, 128 }, info.uv0, (info.uv0 + info.uv1).eval());
                } */

                ImGui::EndTooltip();
              }

              ImGui::SameLine(ImGui::GetContentRegionMax().x - 16.f);

              if (ImGui::SmallButton("X")) {
                debug::check_expr(false, "Not implemented");
              } 
              
              ImGui::TreePop();
            } // if (treenode)
          } // for (i)
        } // if (header)

        if (ImGui::CollapsingHeader(std::format("Illuminant functions ({})", e_scene.resources.illuminants.size()).c_str())) {
          for (const auto &func : e_scene.resources.illuminants) {
            if (ImGui::TreeNodeEx(func.name.c_str(), ImGuiTreeNodeFlags_Leaf)) {
              ImGui::SameLine(ImGui::GetContentRegionMax().x - 16.f);

              if (ImGui::SmallButton("X")) {
                debug::check_expr(false, "Not implemented");
              } 
              
              ImGui::TreePop();
            }
          }
        }

        if (ImGui::CollapsingHeader(std::format("Observer functions ({})", e_scene.resources.observers.size()).c_str())) {
          for (const auto &func : e_scene.resources.observers) {
            if (ImGui::CollapsingHeader(func.name.c_str())) {
              // ...
            }
          }
        }

        if (ImGui::CollapsingHeader(std::format("Basis functions ({})", e_scene.resources.bases.size()).c_str())) {
          for (const auto &func : e_scene.resources.bases) {
            if (ImGui::CollapsingHeader(func.name.c_str())) {
              // ...
            }
          }
        }
      }
      ImGui::End();
    }
  };
} // namespace met