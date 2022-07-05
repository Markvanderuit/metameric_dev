#include <small_gl/buffer.hpp>
#include <small_gl/texture.hpp>
#include <small_gl/utility.hpp>
#include <metameric/core/detail/glm.hpp>
#include <metameric/core/spectrum.hpp>
#include <metameric/core/utility.hpp>
#include <metameric/gui/task/viewport_task.hpp>
#include <metameric/gui/task/viewport_draw_task.hpp>
#include <metameric/gui/task/viewport_draw_grid_task.hpp>
#include <metameric/gui/task/viewport_draw_begin_task.hpp>
#include <metameric/gui/task/viewport_draw_end_task.hpp>
#include <metameric/gui/detail/arcball.hpp>
#include <metameric/gui/detail/imgui.hpp>
#include <glm/vec3.hpp>
#include <ImGuizmo.h>
#include <iostream>
#include <functional>
#include <numeric>
#include <ranges>

namespace met {
  namespace detail {
    // Convert a world-space vector to screen space in [0, 1]
    glm::vec2 screen_space(const glm::vec3 &v,        // world-space vector
                           const glm::mat4 &mat) {    // camera view/proj matrix
      glm::vec4 trf = mat * glm::vec4(v, 1);
      return glm::vec2(trf) / trf.w * .5f + .5f;
    }

    // Convert a screen-space vector in [0, 1] to window space
    glm::vec2 window_space(const glm::vec2 &v,        // screen-space vector
                           const glm::vec2 &offset,   // window offset
                           const glm::vec2 &size) {   // window size
      return offset + glm::vec2(v.x, 1.f - v.y) * size;
    }

    // Convert a world-space vector to window space
    glm::vec2 window_space(const glm::vec3 &v,        // world-space vector
                           const glm::mat4 &mat,      // camera view/proj matrix
                           const glm::vec2 &offset,   // window offset
                           const glm::vec2 &size) {   // window size
      return window_space(screen_space(v, mat), offset, size);
    }
    
    constexpr auto i_get = [](auto &v) { return [&v](const auto &i) -> auto& { return v[i]; }; };
  } // namespace detail

  ViewportTask::ViewportTask(const std::string &name)
  : detail::AbstractTask(name) { }

  void ViewportTask::init(detail::TaskInitInfo &info) {
    // Create arcball camera, centering around a (0.5, 0.5, 0.5) scene midpoint
    detail::Arcball arcball = {{ .eye = glm::vec3(1.5), .center = glm::vec3(0.5) }};

    // Share resources
    info.insert_resource<detail::Arcball>("viewport_arcball", std::move(arcball));
    info.insert_resource<gl::Texture2d3f>("viewport_texture", gl::Texture2d3f());
    info.insert_resource<glm::mat4>("viewport_model_matrix", glm::mat4(1));

    // Add subtasks after this task
    info.emplace_task<ViewportDrawBeginTask>(name() + "_draw_begin");
    info.emplace_task<ViewportDrawGridTask>(name() + "_draw_grid");
    info.emplace_task<ViewportDrawTask>(name() + "_draw");
    info.emplace_task<ViewportDrawEndTask>(name() + "_draw_end");
  }

  void ViewportTask::eval(detail::TaskEvalInfo &info) {
    // Get shared resources
    auto &i_viewport_texture      = info.get_resource<gl::Texture2d3f>("viewport_texture");
    auto &i_viewport_arcball      = info.get_resource<detail::Arcball>("viewport_arcball");
    auto &i_viewport_model_matrix = info.get_resource<glm::mat4>("viewport_model_matrix");
    auto &e_color_gamut_buffer    = info.get_resource<gl::Buffer>("global", "color_gamut_buffer");

    // Begin window draw; declare scoped ImGui style state
    auto imgui_state = { ImGui::ScopedStyleVar(ImGuiStyleVar_WindowRounding, 16.f), 
                         ImGui::ScopedStyleVar(ImGuiStyleVar_WindowBorderSize, 0.f), 
                         ImGui::ScopedStyleVar(ImGuiStyleVar_WindowPadding, { 0.f, 0.f })};

    ImGui::Begin("Viewport", 0, ImGuiWindowFlags_NoBringToFrontOnFocus);

    // Compute viewport offset and size, minus ImGui's tab bars etc
    auto viewport_offs = static_cast<glm::vec2>(ImGui::GetWindowPos()) 
                       + static_cast<glm::vec2>(ImGui::GetWindowContentRegionMin());
    auto viewport_size = static_cast<glm::vec2>(ImGui::GetWindowContentRegionMax())
                       - static_cast<glm::vec2>(ImGui::GetWindowContentRegionMin());

    // Adjust arcball to viewport's new size
    i_viewport_arcball.m_aspect = viewport_size.x / viewport_size.y;
    i_viewport_arcball.update_matrices(); // TODO redundant

    // (Re-)create viewport texture if necessary; attached framebuffers are resized separately
    if (!i_viewport_texture.is_init() || i_viewport_texture.size() != glm::ivec2(viewport_size)) {
      i_viewport_texture = {{ .size = glm::ivec2(viewport_size) }};
    }

    // Insert image, applying viewport texture to viewport; texture can be safely drawn 
    // to later in the render loop. Flip y-axis UVs to obtain the correct orientation.
    ImGui::Image(ImGui::to_ptr(i_viewport_texture.object()), i_viewport_texture.size(), 
      glm::vec2(0, 1), glm::vec2(1, 0));

    // Generate temporary mapping to color gamut buffer for selection 
    constexpr auto map_flags = gl::BufferAccessFlags::eMapRead | gl::BufferAccessFlags::eMapWrite;
    auto color_gamut_map = convert_span<glm::vec3>(e_color_gamut_buffer.map(map_flags));

    // Handle arcball rotation and gamut selection inside viewport
    auto &io = ImGui::GetIO();
    if (ImGui::IsItemHovered() && !ImGuizmo::IsUsing()) {
      // Apply scroll delta: scroll wheel only for now
      i_viewport_arcball.update_dist_delta(-0.5f * io.MouseWheel);

      // Apply move delta: middle mouse OR left mouse + ctrl
      if (io.MouseDown[2] || (io.MouseDown[0] && io.KeyCtrl)) {
        i_viewport_arcball.update_pos_delta(static_cast<glm::vec2>(io.MouseDelta) / viewport_size);
      }

      // Update camera matrices now that scroll/move delta have been applied
      i_viewport_arcball.update_matrices();

      // Apply selection area: right mouse OR left mouse + shift
      if (io.MouseDown[1]) {
        glm::vec2 ul = io.MouseClickedPos[1], br = io.MousePos, bl(ul.x, br.y), ur(bl.x, ul.y);
        auto col = ImGui::ColorConvertFloat4ToU32(ImGui::GetStyleColorVec4(ImGuiCol_DockingPreview));
        ImGui::GetWindowDrawList()->AddRect(ul, br, col);
        ImGui::GetWindowDrawList()->AddRectFilled(ul, br, col);
      }

      // Right-click-release fixes the selection area; then determine selected gamut position idxs
      if (io.MouseReleased[1]) {
        // Filter tests if a gamut position is inside the selection rectangle in window space
        auto ul = glm::min(glm::vec2(io.MouseClickedPos[1]), glm::vec2(io.MousePos)),
             br = glm::max(glm::vec2(io.MouseClickedPos[1]), glm::vec2(io.MousePos));
        auto is_in_rect = std::views::filter([&](uint i) {
          const glm::vec2 p =  detail::window_space(color_gamut_map[i], 
            i_viewport_arcball.full(), viewport_offs, viewport_size);
          return glm::clamp(p, ul, br) == p;
        });
                 
        // Find and store selected gamut position indices
        m_gamut_selection_indices.clear();
        std::ranges::copy(std::views::iota(0u, color_gamut_map.size()) | is_in_rect, 
          std::back_inserter(m_gamut_selection_indices));
      }

      // Left-click selects a gamut position
      if (io.MouseClicked[0] && !ImGuizmo::IsOver()) {
        // Filter tests if a gamut position is near a clicked position in window space
        auto is_near_click = std::views::filter([&](uint i) {
          glm::vec2 p = detail::window_space(color_gamut_map[i], i_viewport_arcball.full(), 
            viewport_offs, viewport_size);
          return glm::distance(p, glm::vec2(io.MouseClickedPos[0])) <= 8.f;
        });

        // Find and store selected gamut position indices
        m_gamut_selection_indices.clear();
        std::ranges::copy(std::views::iota(0u, color_gamut_map.size()) | is_near_click,
          std::back_inserter(m_gamut_selection_indices));
      }
    }

    // Process transformations to selected gamut positions
    if (!m_gamut_selection_indices.empty()) {
      // Range over selected gamut positions
      const auto gamut_selection = m_gamut_selection_indices 
                                 | std::views::transform(detail::i_get(color_gamut_map));

      // Gizmo anchor position is mean of selected gamut positions
      auto gamut_anchor_pos = std::reduce(gamut_selection.begin(), gamut_selection.end())
                            / static_cast<float>(gamut_selection.size());

      // ImGuizmo manipulator operates on a transform; to obtain translation
      // distance, we transform a point prior to transformation update
      glm::mat4 gamut_anchor_trf = glm::translate(gamut_anchor_pos);
      glm::vec4 gamut_pre_pos    = gamut_anchor_trf * glm::vec4(0, 0, 0, 1);

      // Insert ImGuizmo manipulator at anchor position
      auto rmin = glm::vec2(ImGui::GetWindowPos())
                + glm::vec2(ImGui::GetWindowContentRegionMin()), 
           rmax = glm::vec2(ImGui::GetWindowSize())
                - glm::vec2(ImGui::GetWindowContentRegionMin());
      ImGuizmo::SetRect(rmin.x, rmin.y, rmax.x, rmax.y);
      ImGuizmo::SetDrawlist(ImGui::GetWindowDrawList());
      ImGuizmo::Manipulate(glm::value_ptr(i_viewport_arcball.view()), 
                           glm::value_ptr(i_viewport_arcball.proj()),
                           ImGuizmo::OPERATION::TRANSLATE, 
                           ImGuizmo::MODE::LOCAL, 
                           glm::value_ptr(gamut_anchor_trf));
      
      // After transformation update, we transform a second point to obtain
      // translation distance
      glm::vec4 gamut_post_pos = gamut_anchor_trf * glm::vec4(0, 0, 0, 1);
      glm::vec3 gamut_transl = glm::vec4(gamut_post_pos - gamut_pre_pos);

      // Move selected gamut points by translation distance
      if (ImGuizmo::IsUsing()) {
        std::ranges::for_each(gamut_selection, [&](auto &p) { 
          p = glm::clamp(p + gamut_transl, 0.f, 1.f);
        });
      }
    }
    
    ImGui::End();
    
    // Close buffer mapping
    e_color_gamut_buffer.unmap();
    gl::sync::memory_barrier(gl::BarrierFlags::eClientMappedBuffer);
  }
} // namespace met