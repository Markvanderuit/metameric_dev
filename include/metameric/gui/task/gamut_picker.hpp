#pragma once

#include <small_gl/buffer.hpp>
#include <metameric/core/detail/glm.hpp>
#include <metameric/core/utility.hpp>
#include <metameric/gui/detail/imgui.hpp>
#include <metameric/gui/detail/linear_scheduler/task.hpp>
#include <numeric>

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

        // Plot spectra
        ImGui::PlotLines("reflectance 0", spectra[0].data(), wavelength_samples, 0,
          nullptr, 0.f, 1.f, viewport_size * glm::vec2(.67f, 0.2f));
        ImGui::PlotLines("reflectance 1", spectra[1].data(), wavelength_samples, 0,
          nullptr, 0.f, 1.f, viewport_size * glm::vec2(.67f, 0.2f));
        ImGui::PlotLines("reflectance 2", spectra[2].data(), wavelength_samples, 0,
          nullptr, 0.f, 1.f, viewport_size * glm::vec2(.67f, 0.2f));
        ImGui::PlotLines("reflectance 3", spectra[3].data(), wavelength_samples, 0,
          nullptr, 0.f, 1.f, viewport_size * glm::vec2(.67f, 0.2f));
      }
      ImGui::End();
    }
  };
} // namespace met