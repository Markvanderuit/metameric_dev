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
  namespace detail {
    // just a dot product over arbitrary types
    Color from_barycentric(std::span<Color> input, const eig::Vector4f &v) {
      Color t = 0.f;
      for (size_t i = 0; i < 4; ++i) {
        t += input[i] * v[i];
      }
      return t;
    }

    // just a dot product over arbitrary types
    Spec from_barycentric(std::span<Spec> input, const eig::Vector4f &v) {
      Spec t = 0.f;
      for (size_t i = 0; i < 4; ++i) {
        t += input[i] * v[i];
      }
      return t;
    }

    eig::Vector4f to_barycentric(std::span<Color> gamut, const Color &c) {
      // Obtain matrix T
      auto T = (eig::Matrix3f() << gamut[0] - gamut[3], 
                                   gamut[1] - gamut[3], 
                                   gamut[2] - gamut[3]).finished();

      // Perform barycentric transformation
      auto abc = T.inverse() * (c - gamut[3]).matrix();
      return (eig::Vector4f() << abc, (1.f - abc.sum())).finished().eval();
    };
  } // namespace detail

  class GamutPickerTask : public detail::AbstractTask {
    glm::vec3 m_gamut_mean;
    
  public:
    GamutPickerTask(const std::string &name)
    : detail::AbstractTask(name) { }

    void init(detail::TaskInitInfo &info) override {
      // Compute mean to rotate around
      auto &e_color_gamut_buffer = info.get_resource<gl::Buffer>("global", "color_gamut_buffer");
      auto gamut_map = convert_span<glm::vec3>(e_color_gamut_buffer.map(gl::BufferAccessFlags::eMapRead));
      m_gamut_mean = std::reduce(gamut_map.begin(), gamut_map.end()) / static_cast<float>(gamut_map.size());
      e_color_gamut_buffer.unmap();
    }

    void eval(detail::TaskEvalInfo &info) override {
      // Get externally shared resources
      auto &e_color_gamut_buffer = info.get_resource<gl::Buffer>("global", "color_gamut_buffer");
      auto &e_spectral_gamut_map = info.get_resource<std::span<Spec>>("mapping", "spectral_gamut_map");

      // Generate temporary mapping to color gamut buffer 
      constexpr auto map_flags = gl::BufferAccessFlags::eMapRead | gl::BufferAccessFlags::eMapWrite;
      auto color_gamut_map = convert_span<Color>(e_color_gamut_buffer.map(map_flags));

      // Quick temporary window to modify gamut points
      if (ImGui::Begin("Gamut picker")) {
        ImGui::ColorEdit3("Color 0", color_gamut_map[0].data(), ImGuiColorEditFlags_Float);
        ImGui::ColorEdit3("Color 1", color_gamut_map[1].data(), ImGuiColorEditFlags_Float);
        ImGui::ColorEdit3("Color 2", color_gamut_map[2].data(), ImGuiColorEditFlags_Float);
        ImGui::ColorEdit3("Color 3", color_gamut_map[3].data(), ImGuiColorEditFlags_Float);
      }
      ImGui::End();

      // Quick temporary window to show nearest spectra in the local grid
      if (ImGui::Begin("Gamut sd viewer")) {
        const auto viewport_size = static_cast<glm::vec2>(ImGui::GetWindowContentRegionMax())
                                 - static_cast<glm::vec2>(ImGui::GetWindowContentRegionMin());
        
        constexpr uint grid_size = 64;
        constexpr auto col_to_grid = [&](const glm::vec3 &v) {
          return glm::uvec3(glm::clamp(v, 0.f, 1.f) * static_cast<float>(grid_size - 1));
        };
        constexpr auto grid_to_idx = [&](const glm::uvec3 &u) {
          return u.z * std::pow<uint>(grid_size, 2) + u.y * grid_size + u.x;
        };

        // Obtain colors at gamut's point positions
        std::vector<Color> spectra_to_colors(4);
        std::ranges::transform(e_spectral_gamut_map, spectra_to_colors.begin(),
          [](const auto &s) { return xyz_to_srgb(reflectance_to_xyz(s)); });

        // Plot spectra
        ImGui::PlotLines("reflectance 0", e_spectral_gamut_map[0].data(), wavelength_samples, 0,
          nullptr, 0.f, 1.f, viewport_size * glm::vec2(.67f, 0.125f));
        ImGui::ColorEdit3("color 0, coordinates", color_gamut_map[0].data());
        ImGui::ColorEdit3("color 0, actual", spectra_to_colors[0].data());
        ImGui::PlotLines("reflectance 1", e_spectral_gamut_map[1].data(), wavelength_samples, 0,
          nullptr, 0.f, 1.f, viewport_size * glm::vec2(.67f, 0.125f));
        ImGui::ColorEdit3("color 1, coordinates", color_gamut_map[1].data());
        ImGui::ColorEdit3("color 1, actual", spectra_to_colors[1].data());
        ImGui::PlotLines("reflectance 2", e_spectral_gamut_map[2].data(), wavelength_samples, 0,
          nullptr, 0.f, 1.f, viewport_size * glm::vec2(.67f, 0.125f));
        ImGui::ColorEdit3("color 2, coordinates", color_gamut_map[2].data());
        ImGui::ColorEdit3("color 2, actual", spectra_to_colors[2].data());
        ImGui::PlotLines("reflectance 3", e_spectral_gamut_map[3].data(), wavelength_samples, 0,
          nullptr, 0.f, 1.f, viewport_size * glm::vec2(.67f, 0.125f));
        ImGui::ColorEdit3("color 3, coordinates", color_gamut_map[3].data());
        ImGui::ColorEdit3("color 3, actual", spectra_to_colors[3].data());
      }
      ImGui::End();

      // Close buffer mapping
      e_color_gamut_buffer.unmap();
      gl::sync::memory_barrier(gl::BarrierFlags::eClientMappedBuffer);
    
      /* // Open buffer mapping as vec3
      auto vec3_gamut_map = convert_span<glm::vec3>(e_color_gamut_buffer.map(map_flags));
      
      // Rotate around mean on Y-axis for shits and giggles
      auto t_mean = glm::translate(m_gamut_mean) 
                  * glm::rotate(glm::radians(.5f), glm::vec3(0, 1, 0))
                  * glm::translate(-m_gamut_mean);
      for (auto &v : vec3_gamut_map) {
        v = t_mean * glm::vec4(v, 1);
      }

      // Close buffer mapping
      e_color_gamut_buffer.unmap();
      gl::sync::memory_barrier(gl::BarrierFlags::eClientMappedBuffer); */
    }
  };
} // namespace met