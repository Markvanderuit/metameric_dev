#pragma once

#include <metameric/core/spectrum.hpp>
#include <metameric/core/state.hpp>
#include <metameric/core/utility.hpp>
#include <metameric/core/detail/trace.hpp>
#include <metameric/core/detail/scheduler_task.hpp>
#include <metameric/components/views/detail/imgui.hpp>
#include <metameric/components/views/detail/arcball.hpp>
#include <ImGuizmo.h>
#include <algorithm>
#include <functional>
#include <numeric>
#include <ranges>
#include <optional>
#include <limits>

namespace met {
  namespace detail {
    struct Ray { eig::Vector3f o, d; };

    struct VertexQuery {
      uint i;
      float t;
    };

    struct ElementQuery {
      uint i;
      float t;
    };

    Ray generate_ray(const  Arcball       &cam,
                     const  eig::Vector2f &screen_pos) {
      const float tanf = std::tanf(cam.m_fov_y * .5f);
      const auto &view_inv = cam.view().inverse();
      
      eig::Vector2f s = (screen_pos.array() - .5f) * 2.f;
      eig::Vector3f o = view_inv * eig::Vector3f::Zero();
      eig::Vector3f d = (view_inv * eig::Vector3f(s.x() * tanf * cam.m_aspect, 
                                                  s.y() * tanf, 
                                                  -1) - o).normalized();

      return { o, d };
    }

    std::optional<VertexQuery> rt_nearest_vertex(const Ray                       &ray,
                                                 const std::vector<eig::Array3f> &verts,
                                                       float                      min_distance) {
      float t = std::numeric_limits<float>::max();
      std::optional<VertexQuery> query;

      for (uint i = 0; i < verts.size(); ++i) {
        eig::Vector3f v = verts[i];
        float         t_ = (v - ray.o).dot(ray.d);
        guard_continue(t_ >= 0.f && t_ < t);

        eig::Vector3f x = ray.o + t_ * ray.d;
        guard_continue((v - x).matrix().norm() <= min_distance);

        t = t_;
        query = { i, t };
      }

      return query;
    }

    std::optional<VertexQuery> rt_nearest_triangle(const Ray &ray,
                                                   const std::vector<eig::Array3f> &verts,
                                                   const std::vector<eig::Array3u> &elems) {
      float t = std::numeric_limits<float>::max();
      std::optional<VertexQuery> query;

      for (uint i = 0; i < elems.size(); ++i) {
        // Load triangle data
        const eig::Array3u e = elems[i];
        eig::Vector3f a = verts[e[0]], b = verts[e[1]], c = verts[e[2]];

        // Compute edges, plane normal, triangle centroid
        eig::Vector3f ab = b - a, bc = c - b, ca = a - c;
        eig::Vector3f n  = bc.cross(ab).normalized(),
                      p  = (a + b + c) / 3.f;
        
        // Find intersection point with triangle's plane
        float n_dot_d = n.dot(ray.d);
        guard_continue(std::abs(n_dot_d) >= 0.00001f);

        float t_ = (p - ray.o).dot(n) / n_dot_d;
        guard_continue(t_ >= 0.f && t_ < t);

        // Test if intersection point lies within triangle
        eig::Vector3f x = ray.o + t_ * ray.d;
        guard_continue(n.dot((x - a).cross(ab)) >= 0.f);
        guard_continue(n.dot((x - b).cross(bc)) >= 0.f);
        guard_continue(n.dot((x - c).cross(ca)) >= 0.f);

        t = t_;
        query = { i, t };
      }

      return query;  
    }
  } // namespace detail 

  class ViewportInputTask : public detail::AbstractTask {
    bool                 m_is_gizmo_used;
    std::vector<Colr>    m_gamut_prev;

  public:
    ViewportInputTask(const std::string &name)
    : detail::AbstractTask(name, true) { }

    void init(detail::TaskInitInfo &info) override {
      met_trace_full();
    
      // Get shared resources
      auto &e_gamut_verts   = info.get_resource<ApplicationData>(global_key, "app_data").project_data.gamut_colr_i;

      // Local state
      m_gamut_prev = e_gamut_verts ; // Store a copy of the initial gamut as previous state
      m_is_gizmo_used = false;    // Start with gizmo inactive
      
      // Share resources
      info.insert_resource<std::vector<uint>>("gamut_selection", { });
      info.insert_resource<std::vector<uint>>("gamut_vert_selection", { });
      info.insert_resource<std::vector<uint>>("gamut_elem_selection", { });
      info.emplace_resource<detail::Arcball>("arcball", { .e_eye = 1.5f, .e_center = 0.5f });
    }
    
    void dstr(detail::TaskDstrInfo &info) override {
      met_trace_full();
    }

    void eval(detail::TaskEvalInfo &info) override {
      met_trace_full();

      // Declare scoped ImGui style state
      auto imgui_state = { ImGui::ScopedStyleVar(ImGuiStyleVar_WindowRounding, 16.f), 
                           ImGui::ScopedStyleVar(ImGuiStyleVar_WindowBorderSize, 0.f), 
                           ImGui::ScopedStyleVar(ImGuiStyleVar_WindowPadding, { 0.f, 0.f })};
                           
      // If window is not hovered, exit early
      guard(ImGui::IsItemHovered());

      // If gizmo is not active, handle selection and camera rotation
      if (!ImGuizmo::IsUsing()) {
        eval_camera(info);
        eval_select(info);
      }

      // If a selection is active, handle gizmo
      eval_gizmo(info);
    }

    void eval_camera(detail::TaskEvalInfo &info) {
      met_trace_full();

      // Get shared resources
      auto &io        = ImGui::GetIO();
      auto &i_arcball = info.get_resource<detail::Arcball>("arcball");

      // Compute viewport size minus ImGui's tab bars etc
      eig::Array2f viewport_size = static_cast<eig::Array2f>(ImGui::GetWindowContentRegionMax())
                                 - static_cast<eig::Array2f>(ImGui::GetWindowContentRegionMin());
      
      // Update camera info: aspect ratio, scroll delta, move delta
      i_arcball.m_aspect = viewport_size.x() / viewport_size.y();
      i_arcball.set_dist_delta(-0.5f * io.MouseWheel);
      if (io.MouseDown[2] || (io.MouseDown[0] && io.KeyCtrl)) {
        i_arcball.set_pos_delta(eig::Array2f(io.MouseDelta) / viewport_size.array());
      }
      i_arcball.update_matrices();
    }

    void eval_select(detail::TaskEvalInfo &info) {
      met_trace_full();
      
      // Get shared resources
      auto &io               = ImGui::GetIO();
      auto &i_arcball        = info.get_resource<detail::Arcball>("arcball");
      auto &i_gamut_ind      = info.get_resource<std::vector<uint>>("gamut_selection");
      auto &i_vert_selection = info.get_resource<std::vector<uint>>("gamut_vert_selection");
      auto &i_elem_selection = info.get_resource<std::vector<uint>>("gamut_elem_selection");
      auto &e_app_data       = info.get_resource<ApplicationData>(global_key, "app_data");
      auto &e_proj_data      = e_app_data.project_data;
      auto &e_gamut_verts    = e_app_data.project_data.gamut_colr_i;
      auto &e_gamut_elems    = e_proj_data.gamut_elems;

      // Compute viewport offset and size, minus ImGui's tab bars etc
      eig::Array2f viewport_offs = static_cast<eig::Array2f>(ImGui::GetWindowPos()) 
                                 + static_cast<eig::Array2f>(ImGui::GetWindowContentRegionMin());
      eig::Array2f viewport_size = static_cast<eig::Array2f>(ImGui::GetWindowContentRegionMax())
                                 - static_cast<eig::Array2f>(ImGui::GetWindowContentRegionMin());

      // Get list of vertices nearby mouse, and front-most triangle on mouse
      // auto near_vert_filt = std::views::filter([&](uint i) {
      //   eig::Array2f p = eig::world_to_window_space(e_gamut_verts[i], i_arcball.full(), viewport_offs, viewport_size);
      //   return (p.matrix() - eig::Vector2f(io.MousePos)).norm() < 8.f;
      // });
      // auto near_vert_range = std::views::iota(0u, e_gamut_verts .size()) | near_vert_filt;

      auto ray = detail::generate_ray(i_arcball, eig::window_to_screen_space(io.MousePos, viewport_offs, viewport_size));
      i_vert_selection.clear();
      i_elem_selection.clear();
      if (auto query = detail::rt_nearest_triangle(ray, e_gamut_verts, e_gamut_elems)) {
        fmt::print("{}\n", e_gamut_elems[query->i]);
        i_elem_selection = { query->i };
      }
      if (auto query = detail::rt_nearest_vertex(ray, e_gamut_verts, 0.025f)) {
        // fmt::print("{}\n", query->i);
        i_vert_selection = { query->i };
      }

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
          eig::Array2f p = eig::world_to_window_space(e_gamut_verts[i], i_arcball.full(), viewport_offs, viewport_size);
          return p.max(ul).min(br).isApprox(p);
        });
                  
        // Find and store selected gamut position indices
        i_gamut_ind.clear();
        std::ranges::copy(std::views::iota(0u, e_gamut_verts .size()) | is_in_rect, std::back_inserter(i_gamut_ind));
      }

      // Left-click selects a gamut position
      if (io.MouseClicked[0] && (i_gamut_ind.empty() || !ImGuizmo::IsOver())) {
        // Filter tests if a gamut position is near a clicked position in window space
        auto is_near_click = std::views::filter([&](uint i) {
          eig::Array2f p = eig::world_to_window_space(e_gamut_verts [i], i_arcball.full(), viewport_offs, viewport_size);
          return (p.matrix() - eig::Vector2f(io.MouseClickedPos[0])).norm() < 8.f;
        });

        // Find and store selected gamut position indices
        i_gamut_ind.clear();
        std::ranges::copy(std::views::iota(0u, e_gamut_verts .size()) | is_near_click, std::back_inserter(i_gamut_ind));

        // Quick test hack; subdivide triangle forcibly
        for (uint i : i_elem_selection) {
          eig::Array3u el = e_gamut_elems[i];
          
          // Add new vertex
          Colr c = (e_gamut_verts[el[0]] + e_gamut_verts[el[1]] + e_gamut_verts[el[2]]) / 3.f;
          e_gamut_verts.push_back(c);
          e_proj_data.gamut_offs_j.push_back(Colr(0.f));
          e_proj_data.gamut_mapp_i.push_back(0);
          e_proj_data.gamut_mapp_j.push_back(1);

          // Replace current element by three new elements
          uint j = e_gamut_verts.size() - 1;
          e_gamut_elems.erase(e_gamut_elems.begin() + i);
          e_gamut_elems.push_back(eig::Array3u(el[0], el[1], j));
          e_gamut_elems.push_back(eig::Array3u(el[1], el[2], j));
          e_gamut_elems.push_back(eig::Array3u(el[2], el[0], j));

          // This invalidates iteration, so break accordingly
          break;
        }
      }
    }
    
    void eval_gizmo(detail::TaskEvalInfo &info) {
      met_trace_full();
      
      constexpr
      auto i_get = [](auto &v) { return [&v](const auto &i) -> auto& { return v[i]; }; };
      
      // Get shared resources
      auto &io          = ImGui::GetIO();
      auto &i_arcball   = info.get_resource<detail::Arcball>("arcball");
      auto &e_app_data  = info.get_resource<ApplicationData>(global_key, "app_data");
      auto &e_gamut_verts  = e_app_data.project_data.gamut_colr_i;
      auto &i_gamut_ind = info.get_resource<std::vector<uint>>("gamut_selection");

      // Range over selected gamut positions
      const auto gamut_selection = i_gamut_ind | std::views::transform(i_get(e_gamut_verts ));

      // Gizmo anchor position is mean of selected gamut positions
      eig::Vector3f gamut_anchor_pos = std::reduce(range_iter(gamut_selection), Colr(0.f))
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
        m_gamut_prev = e_gamut_verts ;
      }

      // Halfway gizmo drag
      if (ImGuizmo::IsUsing()) {
        // Get range view over gamut components affected by the translation; then apply translation
        const auto move_selection = i_gamut_ind | std::views::transform(i_get(e_gamut_verts ));
        std::ranges::for_each(move_selection, [&](auto &p) { p = (p + gamut_transl.array()).min(1.f).max(0.f); });
      }

      // End gizmo drag
      if (!ImGuizmo::IsUsing() && m_is_gizmo_used) {
        m_is_gizmo_used = false;
        
        // Register data edit as drag finishes
        e_app_data.touch({ 
          .name = "Move gamut points", 
          .redo = [edit = e_gamut_verts ](auto &data) { data.gamut_colr_i = edit; }, 
          .undo = [edit = m_gamut_prev](auto &data) { data.gamut_colr_i = edit; }
        });
      }
    }
  };
} // namespace met