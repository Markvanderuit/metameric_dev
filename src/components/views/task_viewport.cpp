#include <metameric/core/math.hpp>
#include <metameric/core/state.hpp>
#include <metameric/core/utility.hpp>
#include <metameric/core/detail/trace.hpp>
#include <metameric/components/views/task_viewport.hpp>
#include <metameric/components/views/viewport/task_draw_begin.hpp>
#include <metameric/components/views/viewport/task_draw_end.hpp>
#include <metameric/components/views/viewport/task_draw.hpp>
#include <metameric/components/views/viewport/task_draw_grid.hpp>
#include <metameric/components/views/viewport/task_draw_points.hpp>
#include <metameric/components/views/detail/imgui.hpp>
#include <metameric/components/views/detail/arcball.hpp>
#include <small_gl/buffer.hpp>
#include <small_gl/texture.hpp>
#include <small_gl/utility.hpp>
#include <ImGuizmo.h>
#include <algorithm>
#include <functional>
#include <iostream>
#include <numeric>
#include <ranges>

namespace met {
  namespace detail {
    // Convert a world-space vector to screen space in [0, 1]
    eig::Vector2f screen_space(const eig::Vector3f     &v,      // world-space vector
                               const eig::Projective3f &mat) {  // camera view/proj matrix
      eig::Array4f trf = mat * (eig::Vector4f() << v, 1).finished();
      return trf.head<2>() / trf.w() * .5f + .5f;
    }

    // Convert a screen-space vector in [0, 1] to window space
    eig::Vector2f window_space(const eig::Array2f &v,      // screen-space vector
                               const eig::Array2f &offs,   // window offset
                               const eig::Array2f &size) { // window size
      return offs + size * eig::Array2f(v.x(), 1.f - v.y());
    }

    // Convert a world-space vector to window space
    eig::Vector2f window_space(const eig::Vector3f     &v,      // world-space vector
                               const eig::Projective3f &mat,    // camera view/proj matrix
                               const eig::Vector2f     &offs,   // window offset
                               const eig::Vector2f     &size) { // window size
      return window_space(screen_space(v, mat), offs, size);
    }
    
    constexpr auto i_get = [](auto &v) { return [&v](const auto &i) -> auto& { return v[i]; }; };
  } // namespace detail

  const static std::string draw_begin_name = "_draw_begin";
  const static std::string draw_grid_name  = "_draw_grid";
  const static std::string draw_ocs_name   = "_draw_ocs";
  const static std::string draw_name       = "_draw";
  const static std::string draw_end_name   = "_draw_end";

  void ViewportTask::eval_camera(detail::TaskEvalInfo &info) {
    met_trace_full();
    
    // Get shared resources
    auto &i_arcball = info.get_resource<detail::Arcball>("arcball");
    auto &io        = ImGui::GetIO();

    // Compute viewport size minus ImGui's tab bars etc
    auto viewport_size = static_cast<eig::Vector2f>(ImGui::GetWindowContentRegionMax())
                       - static_cast<eig::Vector2f>(ImGui::GetWindowContentRegionMin());
    
    // Adjust arcball to viewport's new size
    i_arcball.m_aspect = viewport_size.x() / viewport_size.y();

    // Apply scroll delta: scroll wheel only for now
    i_arcball.set_dist_delta(-0.5f * io.MouseWheel);

    // Apply move delta: middle mouse OR left mouse + ctrl
    if (io.MouseDown[2] || (io.MouseDown[0] && io.KeyCtrl)) {
      i_arcball.set_pos_delta(eig::Array2f(io.MouseDelta) / viewport_size.array());
    }
    
    i_arcball.update_matrices();
  }

  void ViewportTask::eval_select(detail::TaskEvalInfo &info) {
    met_trace_full();
    
    // Get shared resources
    auto &i_arcball   = info.get_resource<detail::Arcball>("arcball");
    auto &e_app_data  = info.get_resource<ApplicationData>(global_key, "app_data");
    auto &e_rgb_gamut = e_app_data.project_data.rgb_gamut;
    auto &io          = ImGui::GetIO();

    // Compute viewport offset and size, minus ImGui's tab bars etc
    auto viewport_offs = (static_cast<eig::Array2f>(ImGui::GetWindowPos()) 
                       + static_cast<eig::Array2f>(ImGui::GetWindowContentRegionMin())).eval();
    auto viewport_size = (static_cast<eig::Array2f>(ImGui::GetWindowContentRegionMax())
                       - static_cast<eig::Array2f>(ImGui::GetWindowContentRegionMin())).eval();

    // Apply selection area: right mouse OR left mouse + shift
    if (io.MouseDown[1]) {
      eig::Array2f ul = io.MouseClickedPos[1], br = io.MousePos;
      auto col = ImGui::ColorConvertFloat4ToU32(ImGui::GetStyleColorVec4(ImGuiCol_DockingPreview));
      ImGui::GetWindowDrawList()->AddRect(ul, br, col);
      ImGui::GetWindowDrawList()->AddRectFilled(ul, br, col);
    }

    // Right-click-release fixes the selection area; then determine selected gamut position idxs
    if (io.MouseReleased[1]) {
      // Filter tests if a gamut position is inside the selection rectangle in window space
      auto ul = eig::Array2f(io.MouseClickedPos[1]).min(eig::Array2f(io.MousePos)).eval();
      auto br = eig::Array2f(io.MouseClickedPos[1]).max(eig::Array2f(io.MousePos)).eval();

      auto is_in_rect = std::views::filter([&](uint i) {
        eig::Array2f p = detail::window_space(e_rgb_gamut[i], i_arcball.full(), viewport_offs, viewport_size);
        return p.max(ul).min(br).isApprox(p);
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
        eig::Array2f p = detail::window_space(e_rgb_gamut[i], i_arcball.full(), viewport_offs, viewport_size);
        return (p.matrix() - eig::Vector2f(io.MouseClickedPos[0])).norm() < 8.f;
      });

      // Find and store selected gamut position indices
      m_gamut_selection_indices.clear();
      std::ranges::copy(std::views::iota(0u, e_rgb_gamut.size()) | is_near_click,
        std::back_inserter(m_gamut_selection_indices));
    }
  }

  void ViewportTask::eval_gizmo(detail::TaskEvalInfo &info) {
    met_trace_full();
    
    // Get shared resources
    auto &i_arcball   = info.get_resource<detail::Arcball>("arcball");
    auto &e_app_data  = info.get_resource<ApplicationData>(global_key, "app_data");
    auto &e_rgb_gamut = e_app_data.project_data.rgb_gamut;
    auto &io          = ImGui::GetIO();

    // Range over selected gamut positions
    const auto gamut_selection = m_gamut_selection_indices 
                               | std::views::transform(detail::i_get(e_rgb_gamut));

    // Gizmo anchor position is mean of selected gamut positions
    eig::Vector3f gamut_anchor_pos = std::reduce(range_iter(gamut_selection), Color(0.f))
                                   / static_cast<float>(gamut_selection.size());
    
    // ImGuizmo manipulator operates on a transform; to obtain translation
    // distance, we transform a point prior to transformation update
    auto gamut_anchor_trf = eig::Affine3f(eig::Translation3f(gamut_anchor_pos));
    auto gamut_pre_pos    = gamut_anchor_trf * eig::Vector3f(0, 0, 0);

    // Insert ImGuizmo manipulator at anchor position
    auto rmin = eig::Vector2f(ImGui::GetWindowPos())  + eig::Vector2f(ImGui::GetWindowContentRegionMin()); 
    auto rmax = eig::Vector2f(ImGui::GetWindowSize()) - eig::Vector2f(ImGui::GetWindowContentRegionMin());
    ImGuizmo::SetRect(rmin.x(), rmin.y(), rmax.x(), rmax.y());
    ImGuizmo::SetDrawlist(ImGui::GetWindowDrawList());
    ImGuizmo::Manipulate(i_arcball.view().data(), 
                         i_arcball.proj().data(),
                         ImGuizmo::OPERATION::TRANSLATE, 
                         ImGuizmo::MODE::LOCAL, 
                         gamut_anchor_trf.data());
    
    // After transformation update, we transform a second point to obtain
    // translation distance
    auto gamut_post_pos = gamut_anchor_trf * eig::Vector3f(0, 0, 0);
    auto gamut_transl   = gamut_post_pos - gamut_pre_pos;

    // Start gizmo drag
    if (ImGuizmo::IsUsing() && !m_is_gizmo_used) {
      m_is_gizmo_used = true;
      m_gamut_prev = e_rgb_gamut;
    }

    // Halfway gizmo drag
    if (ImGuizmo::IsUsing()) {
      // Get range view over gamut components affected by the translation;
      // then apply translation
      const auto move_selection = m_gamut_selection_indices 
                                | std::views::transform(detail::i_get(e_rgb_gamut));
      std::ranges::for_each(move_selection, 
        [&](auto &p) { p = (p + gamut_transl.array()).min(1.f).max(0.f); });
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
    met_trace_full();
    
    // Get shared resources
    auto &e_rgb_gamut  = info.get_resource<ApplicationData>(global_key, "app_data").project_data.rgb_gamut;

    // Store a copy of the initial gamut as previous state
    m_gamut_prev = e_rgb_gamut;

    // Start with gizmo inactive
    m_is_gizmo_used = false;

    // Share resources
    info.emplace_resource<detail::Arcball>("arcball", { .e_eye = 1.5f, .e_center = 0.5f });
    info.insert_resource<gl::Texture2d3f>("draw_texture", gl::Texture2d3f());

    // Add subtasks in reverse order
    info.emplace_task_after<ViewportDrawEndTask>(name(),   name() + draw_end_name);
    info.emplace_task_after<ViewportDrawTask>(name(),      name() + draw_name);
    info.emplace_task_after<ViewportDrawPointsTask>(name(),name() + draw_ocs_name);
    // info.emplace_task_after<ViewportDrawGridTask>(name(),  name() + draw_grid_name);
    info.emplace_task_after<ViewportDrawBeginTask>(name(), name() + draw_begin_name);
  }

  void ViewportTask::dstr(detail::TaskDstrInfo &info) {
    met_trace_full();
    
    // Remove subtasks
    info.remove_task(name() + draw_begin_name);
    // info.remove_task(name() + draw_grid_name);
    info.remove_task(name() + draw_ocs_name);
    info.remove_task(name() + draw_name);
    info.remove_task(name() + draw_end_name);
  }

  void ViewportTask::eval(detail::TaskEvalInfo &info) {
    met_trace_full();
    
    // Get shared resources
    auto &i_draw_texture = info.get_resource<gl::Texture2d3f>("draw_texture");

    // Begin window draw; declare scoped ImGui style state
    auto imgui_state = { ImGui::ScopedStyleVar(ImGuiStyleVar_WindowRounding, 16.f), 
                         ImGui::ScopedStyleVar(ImGuiStyleVar_WindowBorderSize, 0.f), 
                         ImGui::ScopedStyleVar(ImGuiStyleVar_WindowPadding, { 0.f, 0.f })};

    if (ImGui::Begin("Viewport", 0, ImGuiWindowFlags_NoBringToFrontOnFocus)) {
      // Compute viewport size minus ImGui's tab bars etc
      // (Re-)create viewport texture if necessary; attached framebuffers are resized separately
      eig::Array2f viewport_size = static_cast<eig::Array2f>(ImGui::GetWindowContentRegionMax())
                                 - static_cast<eig::Array2f>(ImGui::GetWindowContentRegionMin());
      if (!i_draw_texture.is_init() || (i_draw_texture.size() != viewport_size.cast<uint>()).all()) {
        i_draw_texture = {{ .size = viewport_size.cast<uint>() }};
      }

      // Insert image, applying viewport texture to viewport; texture can be safely drawn 
      // to later in the render loop. Flip y-axis UVs to obtain the correct orientation.
      ImGui::Image(ImGui::to_ptr(i_draw_texture.object()), viewport_size, eig::Vector2f(0, 1), eig::Vector2f(1, 0));

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