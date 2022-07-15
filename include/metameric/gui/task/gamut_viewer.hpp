#pragma once

#include <metameric/core/detail/glm.hpp>
#include <metameric/core/utility.hpp>
#include <metameric/gui/detail/imgui.hpp>
#include <metameric/gui/detail/linear_scheduler/task.hpp>
#include <small_gl/buffer.hpp>
#include <small_gl/utility.hpp>
#include <fmt/ranges.h>
#include <numeric>
#include <span>

namespace met {
class GamutViewerTask : public detail::AbstractTask {
  public:
    GamutViewerTask(const std::string &name)
    : detail::AbstractTask(name) { }

    void init(detail::TaskInitInfo &info) override { }

    void eval(detail::TaskEvalInfo &info) override {
      // Get externally shared resources
      auto &e_spectral_gamut_buffer = info.get_resource<gl::Buffer>("generate_spectral", "spectral_gamut_buffer");
      auto &e_color_gamut_buffer    = info.get_resource<gl::Buffer>("global", "color_gamut_buffer");

      // Open temporary mappings to color/spectral gamut buffer 
      auto color_gamut_map    = convert_span<eig::AlArray3f>(e_color_gamut_buffer.map(gl::BufferAccessFlags::eMapReadWrite));
      auto spectral_gamut_map = convert_span<Spec>(e_spectral_gamut_buffer.map(gl::BufferAccessFlags::eMapReadWrite));

      // Quick temporary window to show nearest spectra in the local grid
      if (ImGui::Begin("Gamut viewer")) {
        const auto viewport_size = static_cast<glm::vec2>(ImGui::GetWindowContentRegionMax())
                                 - static_cast<glm::vec2>(ImGui::GetWindowContentRegionMin());
                                 
        // Obtain colors at gamut's point positions
        std::vector<Color> spectra_to_colors(4);
        std::ranges::transform(spectral_gamut_map, spectra_to_colors.begin(),
          [](const auto &s) { return reflectance_to_color(s, { .cmfs = models::cmfs_srgb }); });


        // Plot spectra
        ImGui::PlotLines("reflectance 0", spectral_gamut_map[0].data(), wavelength_samples, 0,
          nullptr, 0.f, 1.f, viewport_size * glm::vec2(.67f, 0.125f));
        ImGui::ColorEdit3("color 0, coordinates", color_gamut_map[0].data());
        ImGui::ColorEdit3("color 0, actual", spectra_to_colors[0].data());
        ImGui::PlotLines("reflectance 1", spectral_gamut_map[1].data(), wavelength_samples, 0,
          nullptr, 0.f, 1.f, viewport_size * glm::vec2(.67f, 0.125f));
        ImGui::ColorEdit3("color 1, coordinates", color_gamut_map[1].data());
        ImGui::ColorEdit3("color 1, actual", spectra_to_colors[1].data());
        ImGui::PlotLines("reflectance 2", spectral_gamut_map[2].data(), wavelength_samples, 0,
          nullptr, 0.f, 1.f, viewport_size * glm::vec2(.67f, 0.125f));
        ImGui::ColorEdit3("color 2, coordinates", color_gamut_map[2].data());
        ImGui::ColorEdit3("color 2, actual", spectra_to_colors[2].data());
        ImGui::PlotLines("reflectance 3", spectral_gamut_map[3].data(), wavelength_samples, 0,
          nullptr, 0.f, 1.f, viewport_size * glm::vec2(.67f, 0.125f));
        ImGui::ColorEdit3("color 3, coordinates", color_gamut_map[3].data());
        ImGui::ColorEdit3("color 3, actual", spectra_to_colors[3].data());
      }
      ImGui::End();

      // Close temporary mappings
      e_color_gamut_buffer.unmap();
      e_spectral_gamut_buffer.unmap();
    }
  };
} // namespace met