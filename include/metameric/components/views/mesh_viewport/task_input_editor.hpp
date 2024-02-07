#pragma once

#include <metameric/core/scheduler.hpp>
#include <metameric/core/scene.hpp>
#include <metameric/core/utility.hpp>
#include <metameric/render/sensor.hpp>
#include <metameric/render/primitives_query.hpp>
#include <metameric/components/views/detail/imgui.hpp>
#include <small_gl/texture.hpp>
#include <ImGuizmo.h>

namespace met {
  static constexpr float selector_near_distance = 12.f;
  static const     auto  vertex_color_center  = ImGui::ColorConvertFloat4ToU32({ 1.f, 1.f, 1.f, 1.f });
  static const     auto  vertex_color_valid   = ImGui::ColorConvertFloat4ToU32({ .5f, .5f, 1.f, 1.f });
  static const     auto  vertex_color_invalid = ImGui::ColorConvertFloat4ToU32({ 1.f, .5f, .5f, 1.f });

  // Helper class to define active uplifting/constraint selection
  class InputSelection {
    constexpr static uint invalid_data = 0xFFFFFFFF;

  public:
    uint uplifting_i  = invalid_data;
    uint constraint_i = 0;
  
  public:
    bool is_valid() const { return uplifting_i != invalid_data; }

    static InputSelection invalid() { return InputSelection(); }
    
    friend auto operator<=>(const InputSelection &, const InputSelection &) = default;
  };

  namespace detail {
    /* eig::Vector3f barycentric_coords(eig::Vector3f p, 
                                     eig::Vector3f a, 
                                     eig::Vector3f b, 
                                     eig::Vector3f c) {
      met_trace();

      eig::Vector3f ab = b - a, 
                    ac = c - a, 
                    ap = p - a;

      float d00 = ab.dot(ab), d01 = ab.dot(ac), d11 = ac.dot(ac);        
      float d20 = ap.dot(ab), d21 = ap.dot(ac), den = d00 * d11 - d01 * d01;   

      eig::Vector3f v = { 0,  (d11 * d20 - d01 * d21) / den,  (d00 * d21 - d01 * d20) / den };
      v.x() = 1.f - v.y() - v.z();
      return v;
    } */
    
    inline
    eig::Vector3f barycentric_coords(eig::Vector3f p, 
                                     eig::Vector3f a, 
                                     eig::Vector3f b, 
                                     eig::Vector3f c) {
      met_trace();

      eig::Vector3f ab = b - a, ac = c - a;

      float a_tri = std::abs(.5f * ac.cross(ab).norm());
      float a_ab  = std::abs(.5f * (p - a).cross(ab).norm());
      float a_ac  = std::abs(.5f * ac.cross(p - a).norm());
      float a_bc  = std::abs(.5f * (c - p).cross(b - p).norm());

      return (eig::Vector3f(a_bc, a_ac, a_ab) / a_tri).eval();
    }

    template <typename T> struct engaged_t {
      template <typename... Ts>
      constexpr bool operator()(const std::variant<Ts...> &variant) const {
        return std::holds_alternative<T>(variant);
      }
      template <typename... Ts>
      constexpr bool operator()(std::variant<Ts...> variant) const {
        return std::holds_alternative<T>(variant);
      }
    };
    template <typename T> inline constexpr auto engaged = engaged_t<T>{};
    
    template <typename T> struct variant_get_t {
      template <typename... Ts>
      constexpr decltype(auto) operator()(const std::variant<Ts...> &variant) const {
        return std::get<T>(variant);
      }
      template <typename... Ts>
      constexpr decltype(auto) operator()(std::variant<Ts...> variant) const {
        return std::get<T>(variant);
      }
    };
    template <typename T> inline constexpr auto variant_get = variant_get_t<T>{};

    template <typename T>
    inline constexpr auto variant_filter_view = [](rng::viewable_range auto &&r) {
      return r | vws::filter(engaged<T>)
               | vws::transform(variant_get<T>);
    };
  } // namespace detail

  class MeshViewportEditorInputTask : public detail::TaskNode {
    RaySensor         m_query_sensor;
    RayQueryPrimitive m_query_prim;
    RayRecord         m_query_result;
    bool              m_is_gizmo_used;
    SurfaceInfo       m_gizmo_prev_si;

    RayRecord eval_ray_query(SchedulerHandle &info, eig::Vector2f xy) {
      met_trace_full();
      
      // Get handles, shared resources, modified resources
      const auto &e_scene   = info.global("scene").getr<Scene>();
      const auto &e_arcball = info.relative("viewport_input_camera")("arcball").getr<detail::Arcball>();

      // Prepare sensor buffer
      auto camera_ray = e_arcball.generate_ray(xy);
      m_query_sensor.origin    = camera_ray.o;
      m_query_sensor.direction = camera_ray.d;
      m_query_sensor.flush();

      // Run raycast primitive, block for results
      m_query_prim.query(m_query_sensor, e_scene);
      return m_query_prim.data();
    }
    
  public:
    void init(SchedulerHandle &info) override {
      met_trace();

      // Record selection item; by default no selection
      info("selection").set<InputSelection>(InputSelection::invalid());

      m_query_prim    = {{ .cache_handle = info.global("cache") }};
      m_is_gizmo_used = false;
    }
    
    void eval(SchedulerHandle &info) override {
      met_trace();

      // Get handles, shared resources, modified resources
      const auto &e_scene      = info.global("scene").getr<Scene>();
      const auto &e_target     = info.relative("viewport_begin")("lrgb_target").getr<gl::Texture2d4f>();
      const auto &e_arcball    = info.relative("viewport_input_camera")("arcball").getr<detail::Arcball>();
      const auto &io           = ImGui::GetIO();
      const auto &is_selection = info("selection").getr<InputSelection>();

      // Compute viewport offset and size, minus ImGui's tab bars etc
      eig::Array2f viewport_offs = static_cast<eig::Array2f>(ImGui::GetWindowPos()) 
                                  + static_cast<eig::Array2f>(ImGui::GetWindowContentRegionMin());
      eig::Array2f viewport_size = static_cast<eig::Array2f>(ImGui::GetWindowContentRegionMax())
                                  - static_cast<eig::Array2f>(ImGui::GetWindowContentRegionMin());

      // Generate InputSelection for each relevant constraint
      std::vector<InputSelection> viable_selections;
      for (const auto &[i, comp] : enumerate_view(e_scene.components.upliftings)) {
        const auto &uplifting = comp.value; 
        for (const auto &[j, vert] : enumerate_view(uplifting.verts)) {
          guard_continue(vert.is_active);
          guard_continue(std::holds_alternative<DirectSurfaceConstraint>(vert.constraint)
                      || std::holds_alternative<IndirectSurfaceConstraint>(vert.constraint));
          viable_selections.push_back({ .uplifting_i = i, .constraint_i = j });
        }
      }

      // Gather relevant constraints together with enumeration data
      auto viable_verts = viable_selections | vws::transform([&](InputSelection is) { 
        return std::pair { is, e_scene.components.upliftings[is.uplifting_i].value.verts[is.constraint_i] };
      });

      // Draw all visible vertices representing surface constraints
      for (const auto &[is, vert] : viable_verts) {
        // Extract world-space position and validity of visible vertices
        auto [is_valid, si] = std::visit(overloaded {
          [](const SurfaceConstraint auto &cstr) { return std::pair { cstr.is_valid(), cstr.surface }; },
          [](const auto &cstr) { return std::pair { false, SurfaceInfo::invalid() }; }
        }, vert.constraint);

        // Get screen-space position
        eig::Vector2f p_screen 
          = eig::world_to_window_space(si.p, e_arcball.full(), viewport_offs, viewport_size);

        // Clip vertices outside viewport
        guard_continue((p_screen.array() >= viewport_offs).all() && (p_screen.array() <= viewport_offs + viewport_size).all());

        // Draw vertex with special coloring dependent on constraint state
        auto dl = ImGui::GetWindowDrawList();
        dl->AddCircleFilled(p_screen, 8.f, is_valid ? vertex_color_valid : vertex_color_invalid);
        dl->AddCircleFilled(p_screen, 4.f, vertex_color_center);
      }

      // If window is active, handle mouse input
      if (ImGui::IsItemHovered()) {
        // On mouse click, and non-use of the gizmo, find the nearest constraint
        // to the mouse in screen-space and assign it as the new active selection
        if (io.MouseClicked[0]) {
          InputSelection is_nearest = InputSelection::invalid();

          for (const auto &[is, vert] : viable_verts) {
            // Extract surface information from surface constraint
            auto si = std::visit(overloaded {
              [](const SurfaceConstraint auto &cstr) { return cstr.surface; },
              [](const auto &cstr) { return SurfaceInfo::invalid(); }
            }, vert.constraint);

            // Get screen-space position; test distance and continue if we are too far away
            eig::Vector2f p_screen 
              = eig::world_to_window_space(si.p, e_arcball.full(), viewport_offs, viewport_size);
            guard_continue((p_screen - eig::Vector2f(io.MousePos)).norm() <= selector_near_distance);
            
            // The first surviving constraint is a mouseover candidate
            is_nearest = is;
            break;
          }
          
          info("selection").getw<InputSelection>() = is_nearest;
        }
        
        // Reset variable on lack of active selection
        if (!is_selection.is_valid())
          m_is_gizmo_used = false;

        // On an active selection, draw ImGuizmo
        if (is_selection.is_valid()) {
          // Get readable vertex data
          const auto &e_vert = info.global("scene").getr<Scene>()
            .get_uplifting_vertex(is_selection.uplifting_i, is_selection.constraint_i);
          
          // Extract surface information from surface constraint
          auto si = std::visit(overloaded {
            [](const SurfaceConstraint auto &cstr) { return cstr.surface; },
            [](const auto &) { return SurfaceInfo::invalid(); }
          }, e_vert.constraint);

          // ImGuizmo manipulator operates on transforms
          auto trf_vert  = eig::Affine3f(eig::Translation3f(si.p));
          auto trf_delta = eig::Affine3f::Identity();

          // Specify ImGuizmo enabled operation; transl for one vertex, transl/rotate for several
          ImGuizmo::OPERATION op = ImGuizmo::OPERATION::TRANSLATE;

          // Specify ImGuizmo settings for current viewport and insert the gizmo
          ImGuizmo::SetRect(viewport_offs[0], viewport_offs[1], viewport_size[0], viewport_size[1]);
          ImGuizmo::SetDrawlist(ImGui::GetWindowDrawList());
          ImGuizmo::Manipulate(e_arcball.view().data(), e_arcball.proj().data(), 
            op, ImGuizmo::MODE::LOCAL, trf_vert.data(), trf_delta.data());

          // Register gizmo use start; cache current vertex position
          if (ImGuizmo::IsUsing() && !m_is_gizmo_used) {
            m_gizmo_prev_si = si;
            m_is_gizmo_used = true;
          }

          // Register continuous gizmo use
          if (ImGuizmo::IsUsing()) {
            // Apply world-space delta to constraint position
            si.p = trf_delta * si.p;

            // Get screen-space position
            eig::Vector2f p_screen = eig::world_to_screen_space(si.p, e_arcball.full());
            
            // Do a raycast, snapping the world position to the nearest surface
            // on a surface hit, and update the local SurfaceInfo object to accomodate
            m_query_result = eval_ray_query(info, p_screen);
            si = (m_query_result.record.is_valid() && m_query_result.record.is_object())
               ? e_scene.get_surface_info(m_query_result)
               : SurfaceInfo { .p = si.p };

            // Get writable vertex data
            auto &e_vert = info.global("scene").getw<Scene>()
              .get_uplifting_vertex(is_selection.uplifting_i, is_selection.constraint_i);

            // Store world-space position in surface constraint
            // TODO no, replace by surface data
            std::visit(overloaded { [&](SurfaceConstraint auto &cstr) { 
              cstr.surface = si;
            }, [](const auto &cstr) { } }, e_vert.constraint);
          }

          // Register gizmo use end; apply current vertex position to scene savte state
          if (!ImGuizmo::IsUsing() && m_is_gizmo_used) {
            m_is_gizmo_used = false;
            
            // Get screen-space position
            eig::Vector2f p_screen = eig::world_to_screen_space(si.p, e_arcball.full());
            
            info.global("scene").getw<Scene>().touch({
              .name = "Move surface constraint",
              .redo = [si = si, is = is_selection](auto &scene) {
                auto &e_vert = scene.get_uplifting_vertex(is.uplifting_i, is.constraint_i);
                std::visit(overloaded { [&](SurfaceConstraint auto &cstr) { 
                  cstr.surface = si;
                }, [](const auto &cstr) {}}, e_vert.constraint);
              },
              .undo = [si = m_gizmo_prev_si, is = is_selection](auto &scene) {
                auto &e_vert = scene.get_uplifting_vertex(is.uplifting_i, is.constraint_i);
                std::visit(overloaded { [&](SurfaceConstraint auto &cstr) { 
                  cstr.surface = si;
                }, [](const auto &cstr) {}}, e_vert.constraint);
              }
            });
          }
        }
      }
    }
  };
} // namespace met