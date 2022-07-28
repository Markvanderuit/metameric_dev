#include <metameric/core/math.hpp>
#include <metameric/core/state.hpp>
#include <metameric/core/utility.hpp>
#include <metameric/components/views/task_viewport.hpp>
#include <metameric/components/views/viewport/task_draw_begin.hpp>
#include <metameric/components/views/viewport/task_draw_end.hpp>
#include <metameric/components/views/viewport/task_draw.hpp>
#include <metameric/components/views/viewport/task_draw_grid.hpp>
#include <metameric/components/views/detail/imgui.hpp>
#include <metameric/components/views/detail/arcball.hpp>
#include <small_gl/buffer.hpp>
#include <small_gl/texture.hpp>
#include <small_gl/utility.hpp>
#include <glm/vec3.hpp>
#include <ImGuizmo.h>
#include <iostream>
#include <functional>
#include <numeric>
#include <ranges>
#include <fmt/ranges.h>

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

  const static std::string draw_begin_name = "_draw_begin";
  const static std::string draw_grid_name  = "_draw_grid";
  const static std::string draw_name       = "_draw";
  const static std::string draw_end_name   = "_draw_end";

  void ViewportTask::eval_camera(detail::TaskEvalInfo &info) {
    // Get shared resources
    auto &i_arcball = info.get_resource<detail::Arcball>("arcball");
    auto &io        = ImGui::GetIO();

    // Compute viewport size minus ImGui's tab bars etc
    auto viewport_size = static_cast<glm::vec2>(ImGui::GetWindowContentRegionMax())
                       - static_cast<glm::vec2>(ImGui::GetWindowContentRegionMin());
    
    // Adjust arcball to viewport's new size
    i_arcball.m_aspect = viewport_size.x / viewport_size.y;

    // Apply scroll delta: scroll wheel only for now
    i_arcball.update_dist_delta(-0.5f * io.MouseWheel);

    // Apply move delta: middle mouse OR left mouse + ctrl
    if (io.MouseDown[2] || (io.MouseDown[0] && io.KeyCtrl)) {
      i_arcball.update_pos_delta(static_cast<glm::vec2>(io.MouseDelta) / viewport_size);
    }
    
    i_arcball.update_matrices();
  }

  void ViewportTask::eval_select(detail::TaskEvalInfo &info) {
    // Get shared resources
    auto &i_arcball   = info.get_resource<detail::Arcball>("arcball");
    auto &e_app_data  = info.get_resource<ApplicationData>(global_key, "app_data");
    auto &e_rgb_gamut = e_app_data.project_data.rgb_gamut;
    auto &io          = ImGui::GetIO();

    // Compute viewport offset and size, minus ImGui's tab bars etc
    auto viewport_offs = static_cast<glm::vec2>(ImGui::GetWindowPos()) 
                       + static_cast<glm::vec2>(ImGui::GetWindowContentRegionMin());
    auto viewport_size = static_cast<glm::vec2>(ImGui::GetWindowContentRegionMax())
                       - static_cast<glm::vec2>(ImGui::GetWindowContentRegionMin());

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
        // TODO deprecate
        const glm::vec3 v = { e_rgb_gamut[i].x(), e_rgb_gamut[i].y(), e_rgb_gamut[i].z() };
        const glm::vec2 p =  detail::window_space(v, i_arcball.full(), viewport_offs, viewport_size);
        return glm::clamp(p, ul, br) == p;
      });
                
      // Find and store selected gamut position indices
      m_gamut_selection_indices.clear();
      std::ranges::copy(std::views::iota(0u, e_rgb_gamut.size()) | is_in_rect, 
        std::back_inserter(m_gamut_selection_indices));
    }

    // Left-click selects a gamut position
    if (io.MouseClicked[0] && !ImGuizmo::IsOver()) {
      // Filter tests if a gamut position is near a clicked position in window space
      auto is_near_click = std::views::filter([&](uint i) {
        const glm::vec3 v = { e_rgb_gamut[i].x(), e_rgb_gamut[i].y(), e_rgb_gamut[i].z() };
        const glm::vec2 p = detail::window_space(v, i_arcball.full(), viewport_offs, viewport_size);
        return glm::distance(p, glm::vec2(io.MouseClickedPos[0])) <= 8.f;
      });

      // Find and store selected gamut position indices
      m_gamut_selection_indices.clear();
      std::ranges::copy(std::views::iota(0u, e_rgb_gamut.size()) | is_near_click,
        std::back_inserter(m_gamut_selection_indices));
    }
  }

  void ViewportTask::draw_gizmo(detail::TaskEvalInfo &info) {
    auto &io = ImGui::GetIO();

  }

  void ViewportTask::eval_gizmo(detail::TaskEvalInfo &info) {
    // Get shared resources
    auto &i_arcball   = info.get_resource<detail::Arcball>("arcball");
    auto &e_app_data  = info.get_resource<ApplicationData>(global_key, "app_data");
    auto &e_rgb_gamut = e_app_data.project_data.rgb_gamut;
    auto &io          = ImGui::GetIO();

    // Range over selected gamut positions
    const auto gamut_selection = m_gamut_selection_indices 
                               | std::views::transform(detail::i_get(e_rgb_gamut));

    // Gizmo anchor position is mean of selected gamut positions
    eig::Array3f gamut_anchor_pos = std::reduce(gamut_selection.begin(), gamut_selection.end(), Color(0.f))
                                  / static_cast<float>(gamut_selection.size());
    const glm::vec3 v = { gamut_anchor_pos.x(), gamut_anchor_pos.y(), gamut_anchor_pos.z() };
    
    // ImGuizmo manipulator operates on a transform; to obtain translation
    // distance, we transform a point prior to transformation update
    glm::mat4 gamut_anchor_trf = glm::translate(v);
    glm::vec4 gamut_pre_pos    = gamut_anchor_trf * glm::vec4(0, 0, 0, 1);

    // Insert ImGuizmo manipulator at anchor position
    auto rmin = glm::vec2(ImGui::GetWindowPos()) + glm::vec2(ImGui::GetWindowContentRegionMin()), 
         rmax = glm::vec2(ImGui::GetWindowSize()) - glm::vec2(ImGui::GetWindowContentRegionMin());
    ImGuizmo::SetRect(rmin.x, rmin.y, rmax.x, rmax.y);
    ImGuizmo::SetDrawlist(ImGui::GetWindowDrawList());
    ImGuizmo::Manipulate(glm::value_ptr(i_arcball.view()), 
                        glm::value_ptr(i_arcball.proj()),
                        ImGuizmo::OPERATION::TRANSLATE, 
                        ImGuizmo::MODE::LOCAL, 
                        glm::value_ptr(gamut_anchor_trf));
    
    // After transformation update, we transform a second point to obtain
    // translation distance
    glm::vec4 gamut_post_pos = gamut_anchor_trf * glm::vec4(0, 0, 0, 1);
    glm::vec3 gamut_transl = glm::vec4(gamut_post_pos - gamut_pre_pos);
    eig::Array3f _gamut_transl = { gamut_transl.x, gamut_transl.y, gamut_transl.z };

    // Start gizmo drag
    if (ImGuizmo::IsUsing() && !m_is_gizmo_used) {
      m_is_gizmo_used = true;
      m_gamut_prev = e_rgb_gamut;
    }

    // Halfway gizmo drag
    if (ImGuizmo::IsUsing()) {
      const auto move_selection = m_gamut_selection_indices 
                                | std::views::transform(detail::i_get(e_rgb_gamut));
      std::ranges::for_each(move_selection, [&](auto &p) { 
        p = (p + _gamut_transl).min(1.f).max(0.f);
      });
    }

    // End gizmo drag
    if (!ImGuizmo::IsUsing() && m_is_gizmo_used) {
      m_is_gizmo_used = false;
      
      // Register data edit as drag finishes
      e_app_data.touch({ 
        .name = "Move gamut points", 
        .redo = [edit = e_rgb_gamut](auto &data) { data.rgb_gamut = edit; }, 
        .undo = [edit = m_gamut_prev](auto &data) { data.rgb_gamut = edit; }
      });
    }
  }

  ViewportTask::ViewportTask(const std::string &name)
  : detail::AbstractTask(name) { }

  void ViewportTask::init(detail::TaskInitInfo &info) {
    // Get shared resources
    auto &e_rgb_gamut  = info.get_resource<ApplicationData>(global_key, "app_data").project_data.rgb_gamut;

    // Store a copy of the initial gamut as previous state
    m_gamut_prev = e_rgb_gamut;

    // Start with gizmo inactive
    m_is_gizmo_used = false;

    // Share resources
    info.emplace_resource<detail::Arcball>("arcball", { .eye = glm::vec3(1.5), .center = glm::vec3(0.5) });
    info.emplace_resource<gl::Texture2d3f>("draw_texture", { });
    info.insert_resource<glm::mat4>("model_matrix", glm::mat4(1));

    // Add subtasks in reverse order
    info.emplace_task_after<ViewportDrawEndTask>(name(),   name() + draw_end_name);
    info.emplace_task_after<ViewportDrawTask>(name(),      name() + draw_name);
    info.emplace_task_after<ViewportDrawGridTask>(name(),  name() + draw_grid_name);
    info.emplace_task_after<ViewportDrawBeginTask>(name(), name() + draw_begin_name);
  }

  void ViewportTask::dstr(detail::TaskDstrInfo &info) {
    // Remove subtasks
    info.remove_task(name() + draw_begin_name);
    info.remove_task(name() + draw_grid_name);
    info.remove_task(name() + draw_name);
    info.remove_task(name() + draw_end_name);
  }

  void ViewportTask::eval(detail::TaskEvalInfo &info) {
    // Get shared resources
    auto &i_draw_texture = info.get_resource<gl::Texture2d3f>("draw_texture");

    // Begin window draw; declare scoped ImGui style state
    auto imgui_state = { ImGui::ScopedStyleVar(ImGuiStyleVar_WindowRounding, 16.f), 
                         ImGui::ScopedStyleVar(ImGuiStyleVar_WindowBorderSize, 0.f), 
                         ImGui::ScopedStyleVar(ImGuiStyleVar_WindowPadding, { 0.f, 0.f })};

    if (ImGui::Begin("Viewport", 0, ImGuiWindowFlags_NoBringToFrontOnFocus)) {
      // Compute viewport size minus ImGui's tab bars etc
      // (Re-)create viewport texture if necessary; attached framebuffers are resized separately
      auto viewport_size = static_cast<glm::vec2>(ImGui::GetWindowContentRegionMax())
                        - static_cast<glm::vec2>(ImGui::GetWindowContentRegionMin());
      if (!i_draw_texture.is_init() || i_draw_texture.size() != glm::ivec2(viewport_size)) {
        i_draw_texture = {{ .size = glm::ivec2(viewport_size) }};
      }

      // Insert image, applying viewport texture to viewport; texture can be safely drawn 
      // to later in the render loop. Flip y-axis UVs to obtain the correct orientation.
      ImGui::Image(ImGui::to_ptr(i_draw_texture.object()), i_draw_texture.size(), glm::vec2(0, 1), glm::vec2(1, 0));

      // Handle input
      if (ImGui::IsItemHovered()) {
        if (!ImGuizmo::IsUsing()) {
          eval_camera(info);
          eval_select(info);
        }

        if (!m_gamut_selection_indices.empty()) {
          eval_gizmo(info);
        }
      }
    }
    
    ImGui::End();
  }
} // namespace met