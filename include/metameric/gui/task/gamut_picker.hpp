#pragma once

#include <metameric/core/detail/glm.hpp>
#include <metameric/core/utility.hpp>
#include <metameric/gui/detail/imgui.hpp>
#include <metameric/gui/detail/linear_scheduler/task.hpp>
#include <small_gl/buffer.hpp>
#include <fmt/ranges.h>
#include <numeric>
#include <span>

namespace met {
  namespace detail {
    Spectrum eval_grid(uint grid_size, const std::vector<Spectrum> &grid, glm::vec3 pos) {
      auto eval_grid_u = [&](const glm::uvec3 &u) {
        uint i = u.z * std::pow<uint>(grid_size, 2) + u.y * grid_size + u.x;
        return grid[i];
      };
      constexpr auto lerp = [](const auto &a, const auto &b, float t) {
        return a + t * (b - a);
      };

      // Compute nearest positions and an alpha component for interpolation
      pos = glm::clamp(pos, 0.f, 1.f) * static_cast<float>(grid_size - 1);
      glm::uvec3 lower = glm::floor(pos),
                 upper = glm::ceil(pos);
      glm::vec3 alpha = pos - glm::vec3(lower); 

      // Sample the eight nearest positions in the grid
      auto lll = eval_grid_u({ lower.x, lower.y, lower.z }),
           ull = eval_grid_u({ upper.x, lower.y, lower.z }),
           lul = eval_grid_u({ lower.x, upper.y, lower.z }),
           llu = eval_grid_u({ lower.x, lower.y, upper.z }),
           uul = eval_grid_u({ upper.x, upper.y, lower.z }),
           luu = eval_grid_u({ lower.x, upper.y, upper.z }),
           ulu = eval_grid_u({ upper.x, lower.y, upper.z }),
           uuu = eval_grid_u({ upper.x, upper.y, upper.z });

      // Return trilinear interpolation
      return lerp(lerp(lerp(lll, ull, alpha.x),
                       lerp(lul, uul, alpha.x),
                       alpha.y),
                  lerp(lerp(llu, ulu, alpha.x),
                       lerp(luu, uuu, alpha.x),
                       alpha.y),
                  alpha.z);
    }

    // just a dot product over arbitrary types
    Color from_barycentric(std::span<Color> input, const eig::Vector4f &v) {
      Color t = 0.f;
      for (size_t i = 0; i < 4; ++i) {
        t += input[i] * v[i];
      }
      return t;
    }

    // just a dot product over arbitrary types
    Spectrum from_barycentric(std::span<Spectrum> input, const eig::Vector4f &v) {
      Spectrum t = 0.f;
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
    glm::vec3 m_gamut_center;

  public:
    GamutPickerTask(const std::string &name)
    : detail::AbstractTask(name) { }

    void init(detail::TaskInitInfo &info) override {
      // Create initial gamut, and store in gl buffer object
      std::vector<glm::vec3> gamut_initial_vertices = {
        glm::vec3(0.2, 0.2, 0.2),
        glm::vec3(0.5, 0.2, 0.2),
        glm::vec3(0.5, 0.5, 0.2),
        glm::vec3(0.33, 0.33, 0.7)
      };
      
      // Obtain center point over vertices
      m_gamut_center = std::reduce(gamut_initial_vertices.begin(), gamut_initial_vertices.end())
                     / (float) gamut_initial_vertices.size();

      gl::Buffer gamut_buffer = {{ .data = as_typed_span<std::byte>(gamut_initial_vertices),
                                   .flags = gl::BufferCreateFlags::eMapRead
                                          | gl::BufferCreateFlags::eMapWrite
                                          | gl::BufferCreateFlags::eMapPersistent
                                          | gl::BufferCreateFlags::eMapCoherent }};

      // Obtain persistent mapping over gamut buffer's data
      auto gamut_buffer_map = convert_span<glm::vec3>(gamut_buffer.map(
          gl::BufferAccessFlags::eMapRead
        | gl::BufferAccessFlags::eMapWrite
        | gl::BufferAccessFlags::eMapPersistent
        | gl::BufferAccessFlags::eMapCoherent 
      ));

      // Share resources
      info.insert_resource<gl::Buffer>("gamut_buffer", std::move(gamut_buffer));
      info.insert_resource<std::span<glm::vec3>>("gamut_buffer_map", std::move(gamut_buffer_map));
    }

    void eval(detail::TaskEvalInfo &info) override {
      // Get internally shared resources
      auto &i_gamut_buffer_map = info.get_resource<std::span<glm::vec3>>("gamut_buffer_map");
      
      // Get externally shared resources
      auto &e_spectral_grid = info.get_resource<std::vector<Spectrum>>("global", "spectral_grid");

      // Quick temporary window to modify gamut points
      if (ImGui::Begin("Gamut picker")) {
        ImGui::ColorEdit3("Color 0", glm::value_ptr(i_gamut_buffer_map[0]),
          ImGuiColorEditFlags_Float);
        ImGui::ColorEdit3("Color 1", glm::value_ptr(i_gamut_buffer_map[1]),
          ImGuiColorEditFlags_Float);
        ImGui::ColorEdit3("Color 2", glm::value_ptr(i_gamut_buffer_map[2]),
          ImGuiColorEditFlags_Float);
        ImGui::ColorEdit3("Color 3", glm::value_ptr(i_gamut_buffer_map[3]),
          ImGuiColorEditFlags_Float);
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

        // Obtain spectra at gamut's point positions;
        std::vector<Spectrum> spectra(4);
        std::ranges::transform(i_gamut_buffer_map, spectra.begin(),
          [&](const auto &v) { return detail::eval_grid(grid_size, e_spectral_grid, v); });

        // Obtain colors at gamut's point positions
        std::vector<Color> spectra_to_colors(4);
        std::ranges::transform(spectra, spectra_to_colors.begin(),
          [](const auto &s) { return xyz_to_srgb(reflectance_to_xyz(s)); });

        // Plot spectra
        ImGui::PlotLines("reflectance 0", spectra[0].data(), wavelength_samples, 0,
          nullptr, 0.f, 1.f, viewport_size * glm::vec2(.67f, 0.2f));
        ImGui::ColorEdit3("color 0, coordinates", glm::value_ptr(i_gamut_buffer_map[0]));
        ImGui::ColorEdit3("color 0, conversion", spectra_to_colors[0].data());
        ImGui::PlotLines("reflectance 1", spectra[1].data(), wavelength_samples, 0,
          nullptr, 0.f, 1.f, viewport_size * glm::vec2(.67f, 0.2f));
        ImGui::ColorEdit3("color 1, coordinates", glm::value_ptr(i_gamut_buffer_map[1]));
        ImGui::ColorEdit3("color 1, conversion", spectra_to_colors[1].data());
        ImGui::PlotLines("reflectance 2", spectra[2].data(), wavelength_samples, 0,
          nullptr, 0.f, 1.f, viewport_size * glm::vec2(.67f, 0.2f));
        ImGui::ColorEdit3("color 2, coordinates", glm::value_ptr(i_gamut_buffer_map[2]));
        ImGui::ColorEdit3("color 2, conversion", spectra_to_colors[2].data());
        ImGui::PlotLines("reflectance 3", spectra[3].data(), wavelength_samples, 0,
          nullptr, 0.f, 1.f, viewport_size * glm::vec2(.67f, 0.2f));
        ImGui::ColorEdit3("color 3, coordinates", glm::value_ptr(i_gamut_buffer_map[3]));
        ImGui::ColorEdit3("color 3, conversion", spectra_to_colors[3].data());

        std::vector<Color> gamut_eigen(4);
        std::ranges::transform(i_gamut_buffer_map, gamut_eigen.begin(),
          [](const glm::vec3 &c) -> Color { return { c.x, c.y, c.z }; });
        Color gamut_average = std::reduce(gamut_eigen.begin(), gamut_eigen.end(), Color(0.f)) / 4.f;
        Spectrum spectrum_average = std::reduce(spectra.begin(), spectra.end(), Spectrum(0.f)) / 4.f;

        fmt::print("Colors:\n\t{}\n\t{}\n\t{}\n\t{}\n\t{}\n",
          gamut_eigen[0], gamut_eigen[1], gamut_eigen[2], gamut_eigen[3], gamut_average);
        
        auto central_coords  = detail::to_barycentric(gamut_eigen, gamut_average);
        auto recovered_spctr = detail::from_barycentric(spectra, central_coords);
        auto recovered_color = xyz_to_srgb(reflectance_to_xyz(recovered_spctr)); 

        ImGui::PlotLines("reflectance n", recovered_spctr.data(), wavelength_samples, 0,
          nullptr, 0.f, 1.f, viewport_size * glm::vec2(.67f, 0.2f));

        // fmt::print("rgb: {}\nspectral: {}\n", recovered_color, color_from_spec);
        fmt::print("rgb: {} recovered through {}\n", recovered_color, central_coords);
      }
      ImGui::End();
    }
  };
} // namespace met