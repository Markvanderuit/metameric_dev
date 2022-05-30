#pragma once

#include <small_gl/buffer.hpp>
#include <metameric/core/detail/glm.hpp>
#include <metameric/core/utility.hpp>
#include <metameric/gui/detail/imgui.hpp>
#include <metameric/gui/detail/linear_scheduler/task.hpp>
#include <numeric>

namespace met {
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
      
      // Apply test rotation around gamut's center
     /*  glm::mat4 rotate_around = glm::rotate(glm::radians(2.f), glm::vec3(0, 1, 0));
      for (auto &v : i_gamut_buffer_map) {
        v = glm::vec3(rotate_around * glm::vec4(v - m_gamut_center, 1)) + m_gamut_center;
      } */

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
    }
  };
} // namespace met