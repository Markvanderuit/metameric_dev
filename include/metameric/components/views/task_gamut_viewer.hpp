#pragma once

#include <metameric/core/math.hpp>
#include <metameric/core/state.hpp>
#include <metameric/core/utility.hpp>
#include <metameric/core/scheduler.hpp>
#include <metameric/components/views/detail/imgui.hpp>
#include <small_gl/buffer.hpp>
#include <small_gl/utility.hpp>
#include <array>

namespace met {
  class GamutViewerTask : public detail::AbstractTask {
  public:
    GamutViewerTask(const std::string &name)
    : detail::AbstractTask(name) { }

    void eval(detail::TaskEvalInfo &info) override {
      // Get externally shared resources
      auto &e_app_data = info.get_resource<ApplicationData>(global_key, "app_data");
      auto &gamut_colr_i  = e_app_data.project_data.gamut_colr_i;
      auto &gamut_spec = e_app_data.project_data.gamut_spec;

      // Quick temporary window to show nearest spectra in the local grid
      if (ImGui::Begin("Gamut viewer")) {
        eig::Array2f viewport_size = static_cast<eig::Array2f>(ImGui::GetWindowContentRegionMax())
                                   - static_cast<eig::Array2f>(ImGui::GetWindowContentRegionMin());

        // Obtain colors at gamut's point positions
        std::array<Colr, 4> spectra_to_colors;
        std::ranges::transform(gamut_spec, spectra_to_colors.begin(),
          [](const auto &s) { return reflectance_to_color(s, { .cmfs = models::cmfs_srgb }).eval(); });
        
        eig::Array2f plot_size = viewport_size * eig::Array2f(.67f, .125f);
        
        // Plot spectra
        ImGui::PlotLines("reflectance 0", gamut_spec[0].data(), wavelength_samples, 0,
          nullptr, 0.f, 1.f, plot_size);
        ImGui::ColorEdit3("color 0, coordinates", gamut_colr_i[0].data(), ImGuiColorEditFlags_Float);
        ImGui::ColorEdit3("color 0, actual", spectra_to_colors[0].data(), ImGuiColorEditFlags_Float);
        ImGui::PlotLines("reflectance 1", gamut_spec[1].data(), wavelength_samples, 0,
          nullptr, 0.f, 1.f, plot_size);
        ImGui::ColorEdit3("color 1, coordinates", gamut_colr_i[1].data(), ImGuiColorEditFlags_Float);
        ImGui::ColorEdit3("color 1, actual", spectra_to_colors[1].data(), ImGuiColorEditFlags_Float);
        ImGui::PlotLines("reflectance 2", gamut_spec[2].data(), wavelength_samples, 0,
          nullptr, 0.f, 1.f, plot_size);
        ImGui::ColorEdit3("color 2, coordinates", gamut_colr_i[2].data(), ImGuiColorEditFlags_Float);
        ImGui::ColorEdit3("color 2, actual", spectra_to_colors[2].data(), ImGuiColorEditFlags_Float);
        ImGui::PlotLines("reflectance 3", gamut_spec[3].data(), wavelength_samples, 0,
          nullptr, 0.f, 1.f, plot_size);
        ImGui::ColorEdit3("color 3, coordinates", gamut_colr_i[3].data(), ImGuiColorEditFlags_Float);
        ImGui::ColorEdit3("color 3, actual", spectra_to_colors[3].data(), ImGuiColorEditFlags_Float);
      }
      ImGui::End();
    }
  };
} // namespace met