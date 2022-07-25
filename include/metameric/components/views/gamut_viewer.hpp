#pragma once

#include <metameric/core/detail/glm.hpp>
#include <metameric/core/state.hpp>
#include <metameric/core/utility.hpp>
#include <metameric/core/scheduler.hpp>
#include <metameric/components/views/detail/imgui.hpp>
#include <small_gl/buffer.hpp>
#include <small_gl/utility.hpp>
#include <array>
#include <numeric>
#include <span>

namespace met {
class GamutViewerTask : public detail::AbstractTask {
  public:
    GamutViewerTask(const std::string &name)
    : detail::AbstractTask(name) { }

    void eval(detail::TaskEvalInfo &info) override {
      // Get externally shared resources
      auto &e_app_data          = info.get_resource<ApplicationData>(global_key, "app_data");
      auto &e_spec_gamut_buffer = info.get_resource<gl::Buffer>("generate_gamut", "spectral_gamut_buffer");

      // Get relevant application data
      std::array<Color, 4> &rgb_gamut = e_app_data.project_data.rgb_gamut;
      std::array<Spec, 4> &spec_gamut = e_app_data.project_data.spec_gamut;

      // Quick temporary window to show nearest spectra in the local grid
      if (ImGui::Begin("Gamut viewer")) {
        const auto viewport_size = static_cast<glm::vec2>(ImGui::GetWindowContentRegionMax())
                                 - static_cast<glm::vec2>(ImGui::GetWindowContentRegionMin());
                                 
        // Obtain colors at gamut's point positions
        std::vector<Color> spectra_to_colors(4);
        std::ranges::transform(spec_gamut, spectra_to_colors.begin(),
          [](const auto &s) { return reflectance_to_color(s, { .cmfs = models::cmfs_srgb }); });
        
        // Plot spectra
        ImGui::PlotLines("reflectance 0", spec_gamut[0].data(), wavelength_samples, 0,
          nullptr, 0.f, 1.f, viewport_size * glm::vec2(.67f, 0.125f));
        ImGui::ColorEdit3("color 0, coordinates", rgb_gamut[0].data());
        ImGui::ColorEdit3("color 0, actual", spectra_to_colors[0].data());
        ImGui::PlotLines("reflectance 1", spec_gamut[1].data(), wavelength_samples, 0,
          nullptr, 0.f, 1.f, viewport_size * glm::vec2(.67f, 0.125f));
        ImGui::ColorEdit3("color 1, coordinates", rgb_gamut[1].data());
        ImGui::ColorEdit3("color 1, actual", spectra_to_colors[1].data());
        ImGui::PlotLines("reflectance 2", spec_gamut[2].data(), wavelength_samples, 0,
          nullptr, 0.f, 1.f, viewport_size * glm::vec2(.67f, 0.125f));
        ImGui::ColorEdit3("color 2, coordinates", rgb_gamut[2].data());
        ImGui::ColorEdit3("color 2, actual", spectra_to_colors[2].data());
        ImGui::PlotLines("reflectance 3", spec_gamut[3].data(), wavelength_samples, 0,
          nullptr, 0.f, 1.f, viewport_size * glm::vec2(.67f, 0.125f));
        ImGui::ColorEdit3("color 3, coordinates", rgb_gamut[3].data());
        ImGui::ColorEdit3("color 3, actual", spectra_to_colors[3].data());
      }
      ImGui::End();
    }
  };
} // namespace met