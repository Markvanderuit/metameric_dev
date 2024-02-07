#pragma once

#include <metameric/core/scheduler.hpp>
#include <metameric/core/scene.hpp>
#include <metameric/core/surface.hpp>
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
    eig::Vector3f gen_barycentric_coords(eig::Vector3f p, 
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

    template <typename T> struct variant_filter_t {};
    template <typename T> inline constexpr auto variant_filter = variant_filter_t<T>{};

    template <typename R, typename T>
    decltype(auto) operator|(R &&r, variant_filter_t<T>) {
      return r | vws::filter(engaged<T>) 
               | vws::transform(variant_get<T>);
    }

    template <typename R, typename T>
    decltype(auto) operator|(R &r, variant_filter_t<T>) {
      return r | vws::filter(engaged<T>) 
               | vws::transform(variant_get<T>);
    }
    
    template <typename R, typename T>
    decltype(auto) operator|(const R &r, variant_filter_t<T>) {
      return r | vws::filter(engaged<T>) 
               | vws::transform(variant_get<T>);
    }

  /*   template <typename T>  */ // struct enumerate_view_t {};
  /*   template <typename T>  */ // inline constexpr auto enumerate_view = enumerate_view_t{};

    /* template <typename R>
    decltype(auto) operator|(R &&r, enumerate_view_t) {
      return vws::iota(0, static_cast<uint>(r.size()))
           | vws::transform([&r](uint i) { return std::pair { i, r[i] }; });
    } */

    inline constexpr auto enumerate_view =
      [](rng::viewable_range auto &&r) {
        return vws::iota(0u, static_cast<uint>(r.size()))
             | vws::transform([&r](uint i) { return std::pair { i, r[i] }; });
      };
  } // namespace detail

  class MeshViewportEditorInputTask : public detail::TaskNode {
    RaySensor         m_query_sensor;
    RayQueryPrimitive m_query_prim;
    bool              m_is_gizmo_used;
    eig::Array3f      m_gizmo_p_prev;

    RayRecord eval_ray_query(SchedulerHandle &info, eig::Vector2f xy) {
      met_trace_full();
      
      // Get handles, shared resources, modified resources
      const auto &e_scene   = info.global("scene").getr<Scene>();
      const auto &e_target  = info.relative("viewport_begin")("lrgb_target").getr<gl::Texture2d4f>();
      const auto &e_arcball = info.relative("viewport_input_camera")("arcball").getr<detail::Arcball>();
      const auto &io        = ImGui::GetIO();
      
      // Compute viewport offset and size, minus ImGui's tab bars etc
      eig::Array2f viewport_offs = static_cast<eig::Array2f>(ImGui::GetWindowPos()) 
                                 + static_cast<eig::Array2f>(ImGui::GetWindowContentRegionMin());
      eig::Array2f viewport_size = static_cast<eig::Array2f>(ImGui::GetWindowContentRegionMax())
                                 - static_cast<eig::Array2f>(ImGui::GetWindowContentRegionMin());

      // Prepare sensor buffer
      auto camera_ray = e_arcball.generate_ray(xy);
      m_query_sensor.origin    = camera_ray.o;
      m_query_sensor.direction = camera_ray.d;
      m_query_sensor.flush();

      // Run raycast primitive, block for results
      m_query_prim.query(m_query_sensor, e_scene);
      return m_query_prim.data();
        
      // if (auto ray = m_query_prim.data(); ray.record.is_valid() && ray.record.is_object()) {
      //   m_query_result = ray;
      // } else {
      //   m_query_result = RayRecord::invalid();
      // }

      // // Given a valid intersection on a object surface
      // if (ray.record.is_valid() && ray.record.is_object()) {
      //   const auto &e_object = e_scene.components.objects[ray.record.object_i()].value;
      //   const auto &e_mesh   = e_scene.resources.meshes[e_object.mesh_i].value();
      //   const auto &e_prim   = e_mesh.elems[ray.record.primitive_i()];
        
      //   // Determine hit position and barycentric coordinates in primitive
      //   auto p    = ray.get_position();
      //   auto bary = detail::gen_barycentric_coords((e_object.transform.affine().inverse() 
      //                                               * eig::Vector4f(p.x(), p.y(), p.z(), 1)).head<3>(),
      //                                              e_mesh.verts[e_prim[0]],
      //                                              e_mesh.verts[e_prim[1]],
      //                                              e_mesh.verts[e_prim[2]]);

      //   fmt::print("{} -> {}\n", p, bary);
      // }
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
      for (const auto &[i, comp] : detail::enumerate_view(e_scene.components.upliftings)) {
        const auto &uplifting = comp.value; 
        for (const auto &[j, vert] : detail::enumerate_view(uplifting.verts)) {
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
        auto [is_valid, p_world] = std::visit(overloaded {
          [](const SurfaceConstraint auto &cstr) { return std::pair { cstr.is_valid(), cstr.surface_p }; },
          [](const auto &cstr) { return std::pair { false, eig::Array3f(0) }; }
        }, vert.constraint);

        // Get screen-space position
        eig::Vector2f p_screen 
          = eig::world_to_window_space(p_world, e_arcball.full(), viewport_offs, viewport_size);

        // Clip vertices outside viewport
        guard_continue((p_screen.array() >= viewport_offs).all() && (p_screen.array() <= viewport_offs + viewport_size).all());

        // Draw vertex with special coloring dependent on constraint state
        auto dl = ImGui::GetWindowDrawList();
        dl->AddCircleFilled(p_screen, 8.f, is_valid           ? vertex_color_valid : vertex_color_invalid);
        dl->AddCircleFilled(p_screen, 4.f, is == is_selection ? vertex_color_valid : vertex_color_center);
      }

      // If window is active, handle mouse input
      if (ImGui::IsItemHovered()) {
        // On mouse click, and non-use of the gizmo, find the nearest constraint
        // to the mouse in screen-space and assign it as the new active selection
        if (io.MouseClicked[0]) {
          InputSelection is_nearest = InputSelection::invalid();

          for (const auto &[is, vert] : viable_verts) {
            // Extract world-space position from surface constraints
            eig::Vector3f p_world = std::visit(overloaded {
              [](const SurfaceConstraint auto &cstr) { return cstr.surface_p; },
              [](const auto &cstr) { return eig::Array3f(0); }
            }, vert.constraint);

            // Get screen-space position; test distance and continue if we are too far away
            eig::Vector2f p_screen 
              = eig::world_to_window_space(p_world, e_arcball.full(), viewport_offs, viewport_size);
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
          
          // Extract world-space position from surface constraints
          eig::Vector3f p_world = std::visit(overloaded {
            [](const SurfaceConstraint auto &cstr) { return cstr.surface_p; },
            [](auto) { return eig::Array3f(0); }
          }, e_vert.constraint);

          // ImGuizmo manipulator operates on transforms
          auto trf_vert  = eig::Affine3f(eig::Translation3f(p_world));
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
            m_gizmo_p_prev  = p_world;
            m_is_gizmo_used = true;
          }

          // Register continuous gizmo use
          if (ImGuizmo::IsUsing()) {
            // Apply world-space delta to constraint position
            p_world = trf_delta * p_world;

            // Get screen-space position
            eig::Vector2f p_screen 
              = eig::world_to_screen_space(p_world, e_arcball.full());
            
            // Do a raycast, snapping the world position to the nearest surface
            // on a surface hit
            if (auto ray = eval_ray_query(info, p_screen); 
                ray.record.is_valid() && ray.record.is_object()) {
              p_world = ray.get_position();
            }

            // Get writable vertex data
            auto &e_vert = info.global("scene").getw<Scene>()
              .get_uplifting_vertex(is_selection.uplifting_i, is_selection.constraint_i);

            // Store world-space position in surface constraint
            std::visit(overloaded {
              [&](SurfaceConstraint auto &cstr) { cstr.surface_p = p_world; },
              [](const auto &cstr) { }
            }, e_vert.constraint);
          }

          // Register gizmo use end; apply current vertex position to scene savte state
          if (!ImGuizmo::IsUsing() && m_is_gizmo_used) {
            m_is_gizmo_used = false;
            
            // Get screen-space position
            eig::Vector2f p_screen 
              = eig::world_to_screen_space(p_world, e_arcball.full());


            // TODO Continue here
            /* // Do a raycast, recording surface data on a hit
            SurfaceRecord record = SurfaceRecord::invalid();
            if (auto ray = eval_ray_query(info, p_screen); 
                ray.record.is_valid() && ray.record.is_object()) {
              record = ray.record;
            } */
            
            info.global("scene").getw<Scene>().touch({
              .name = "Move surface constraint",
              .redo = [p = p_world, is = is_selection](auto &scene) {
                auto &e_vert = scene.get_uplifting_vertex(is.uplifting_i, is.constraint_i);
                std::visit(overloaded {
                  [&](SurfaceConstraint auto &cstr) { cstr.surface_p = p; },
                  [](const auto &cstr) { }
                }, e_vert.constraint);
              },
              .undo = [p = m_gizmo_p_prev, is = is_selection](auto &scene) {
                auto &e_vert = scene.get_uplifting_vertex(is.uplifting_i, is.constraint_i);
                std::visit(overloaded {
                  [&](SurfaceConstraint auto &cstr) { cstr.surface_p = p; },
                  [](const auto &cstr) { }
                }, e_vert.constraint);
              }
            });
          }
        }
      }

      // If a valid result is stored, show test tooltip for now
      // if (m_query_result.record.is_valid()) {
      //   // Compute viewport offset and size, minus ImGui's tab bars etc
      //   eig::Array2f viewport_offs = static_cast<eig::Array2f>(ImGui::GetWindowPos()) 
      //                              + static_cast<eig::Array2f>(ImGui::GetWindowContentRegionMin());
      //   eig::Array2f viewport_size = static_cast<eig::Array2f>(ImGui::GetWindowContentRegionMax())
      //                              - static_cast<eig::Array2f>(ImGui::GetWindowContentRegionMin());

      //   // Get pixel position from query result's world position
      //   auto xy = eig::world_to_window_space(m_query_result.get_position(),
      //                                        e_arcball.full(),
      //                                        viewport_offs,
      //                                        viewport_size);

      //   // Spawn floating dot if pixel position falls within viewport
      //   if (!(xy.array() <= viewport_offs).any() && !(xy.array() >= viewport_offs + viewport_size).any()) {
          
      //     ImGui::GetWindowDrawList()->AddCircleFilled(xy, 8.f, ImGui::ColorConvertFloat4ToU32({ .5f, .5f, 1.f, 1.f }));
      //     ImGui::GetWindowDrawList()->AddCircleFilled(xy, 4.f, ImGui::ColorConvertFloat4ToU32({ 1.f, 1.f, 1.f, 1.f }));

      //     /* ImGui::SetNextWindowPos(xy);
      //     if (ImGui::Begin("Overlay", nullptr, ImGuiWindowFlags_NoDecoration)) {
      //       ImGui::Text("Hi!\n");

      //       // Track mouse input while dragging the thing.
      //       if (io.MouseDown[ImGuiMouseButton_Left]) {
      //         eval_ray_query(info);
      //       }
      //     }
      //     ImGui::End(); */
      //   }
      // }
    }
  };
} // namespace met