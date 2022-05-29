#pragma once

#include <small_gl/buffer.hpp>
#include <small_gl/texture.hpp>
#include <metameric/core/detail/glm.hpp>
#include <metameric/core/utility.hpp>
#include <metameric/gui/detail/imgui.hpp>
#include <metameric/gui/detail/arcball.hpp>
#include <metameric/gui/detail/linear_scheduler/task.hpp>
#include <metameric/gui/task/viewport_pointdraw_task.hpp>
#include <ImGuizmo.h>
#include <iostream>

namespace met {
  class ViewportTask : public detail::AbstractTask {
    std::vector<glm::vec2> m_click_positions;

  public:
    ViewportTask(const std::string &name)
    : detail::AbstractTask(name) { }

    void init(detail::TaskInitInfo &info) override {
      // Create arcball camera, centering around a (0.5, 0.5, 0.5) scene midpoint
      detail::Arcball arcball = {{ .eye = glm::vec3(1.5), .center = glm::vec3(0.5) }};

      // Share resources
      info.insert_resource<detail::Arcball>("viewport_arcball", std::move(arcball));
      info.insert_resource<gl::Texture2d3f>("viewport_texture", gl::Texture2d3f());
      info.insert_resource<glm::mat4>("viewport_model_matrix", glm::mat4(1));

      // Add subtasks
      info.emplace_task<ViewportPointdrawTask>(name() + "_draw");
    }

    void eval(detail::TaskEvalInfo &info) override {
      // Begin window draw; declare scoped ImGui style state
      auto imgui_state = { ImGui::ScopedStyleVar(ImGuiStyleVar_WindowRounding, 16.f), 
                           ImGui::ScopedStyleVar(ImGuiStyleVar_WindowBorderSize, 0.f), 
                           ImGui::ScopedStyleVar(ImGuiStyleVar_WindowPadding, { 0.f, 0.f })};
      ImGui::Begin("Viewport", 0, ImGuiWindowFlags_NoBringToFrontOnFocus);

      // Get internally shared resources
      auto &i_viewport_texture = info.get_resource<gl::Texture2d3f>("viewport_texture");
      auto &i_viewport_arcball = info.get_resource<detail::Arcball>("viewport_arcball");
      auto &i_viewport_model_matrix = info.get_resource<glm::mat4>("viewport_model_matrix");

      // Adjust arcball aspect ratio and (re)create viewport texture if necessary
      auto viewport_size = static_cast<glm::vec2>(ImGui::GetWindowContentRegionMax())
                         - static_cast<glm::vec2>(ImGui::GetWindowContentRegionMin());
      if (!i_viewport_texture.is_init() || i_viewport_texture.size() != glm::ivec2(viewport_size)) {
        i_viewport_texture = {{ .size = glm::ivec2(viewport_size) }};
        i_viewport_arcball.m_aspect = viewport_size.x / viewport_size.y;
        i_viewport_arcball.update_matrices();
      }

      // Insert image, applying viewport texture to viewport; texture is drawn later
      // flip y-axis UVs to obtain the correct orientation
      ImGui::Image(ImGui::to_ptr(i_viewport_texture.object()), i_viewport_texture.size(), 
        glm::vec2(0, 1), glm::vec2(1, 0));

      // Handle arcball rotation
      auto &io = ImGui::GetIO();
      if (ImGui::IsItemHovered() && !ImGuizmo::IsUsing()) {
        if (io.MouseClicked[0]) {
          m_click_positions.push_back(io.MouseClickedPos[0]);
        }        

        // Apply scroll delta
        i_viewport_arcball.update_dist_delta(-0.5f * io.MouseWheel);

        // Apply move delta
        if (io.MouseDown[0]) {
          i_viewport_arcball.update_pos_delta(static_cast<glm::vec2>(io.MouseDelta) / viewport_size);
        }

        // Update camera matrices now that delta has been applied
        i_viewport_arcball.update_matrices();

        // Register click on gamut position
        // TODO: continue here tomorrow
        if (io.MouseClicked[0]) {
          auto &e_gamut_buffer_map = info.get_resource<std::span<glm::vec3>>("gamut_picker", "gamut_buffer_map");
          glm::vec2 clicked_pos = glm::vec2(io.MouseClickedPos[0]) 
                                - glm::vec2(ImGui::GetWindowPos())
                                - glm::vec2(ImGui::GetWindowContentRegionMin());
          glm::vec3 gamut_pos = e_gamut_buffer_map[0];
          glm::vec4 trf_pos = i_viewport_arcball.full() * glm::vec4(gamut_pos, 1);
          glm::vec2 scr_pos = ((glm::vec2(trf_pos) / trf_pos.w) / 2.f + .5f);
          scr_pos = viewport_size * scr_pos;
          scr_pos.y = viewport_size.y - scr_pos.y;
        }
      }

      auto *draw_list = ImGui::GetWindowDrawList();
      for (const auto &v : m_click_positions) {
        draw_list->AddCircleFilled(v, 0.1f, ImGui::ColorConvertFloat4ToU32({ 1.f, 1.f, 1.f, 1.f }));
      }
      
      // Insert ImGuizmo manipulator
      auto rmin = ImGui::GetWindowPos(), rmax = ImGui::GetWindowSize();
      ImGuizmo::SetRect(rmin.x, rmin.y, rmax.x, rmax.y);
      ImGuizmo::SetDrawlist(ImGui::GetWindowDrawList());
      ImGuizmo::Manipulate(glm::value_ptr(i_viewport_arcball.view()), 
                           glm::value_ptr(i_viewport_arcball.proj()),
                           ImGuizmo::OPERATION::TRANSLATE, 
                           ImGuizmo::MODE::LOCAL, 
                           glm::value_ptr(i_viewport_model_matrix));

      // End window draw
      ImGui::End();
    }
  };
} // namespace met