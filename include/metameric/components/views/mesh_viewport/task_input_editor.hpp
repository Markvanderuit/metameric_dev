#pragma once

#include <metameric/core/scheduler.hpp>
#include <metameric/core/scene.hpp>
#include <metameric/core/utility.hpp>
#include <metameric/render/sensor.hpp>
#include <metameric/render/primitives_query.hpp>
#include <metameric/components/views/detail/imgui.hpp>
#include <small_gl/texture.hpp>

namespace met {
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


  } // namespace detail

  class MeshViewportEditorInputTask : public detail::TaskNode {
    RaySensor         m_query_sensor;
    RayQueryPrimitive m_query_prim;
    RayRecord         m_query_result;

    // ...

    void eval_ray_query(SchedulerHandle &info) {
      met_trace();
      
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

      // Generate a camera ray from the current mouse position
      auto screen_pos = eig::window_to_screen_space(io.MousePos, viewport_offs, viewport_size);
      auto camera_ray = e_arcball.generate_ray(screen_pos);
      
      // Prepare sensor buffer
      m_query_sensor.origin    = camera_ray.o;
      m_query_sensor.direction = camera_ray.d;
      m_query_sensor.flush();

      // Run raycast primitive, block for results
      m_query_prim.query(m_query_sensor, e_scene);
      if (auto ray = m_query_prim.data(); ray.record.is_valid() && ray.record.is_object()) {
        m_query_result = ray;
      } else {
        m_query_result = RayRecord::invalid();
      }

      /* // Given a valid intersection on a object surface
      if (ray.record.is_valid() && ray.record.is_object()) {
        const auto &e_object = e_scene.components.objects[ray.record.object_i()].value;
        const auto &e_mesh   = e_scene.resources.meshes[e_object.mesh_i].value();
        const auto &e_prim   = e_mesh.elems[ray.record.primitive_i()];
        
        // Determine hit position and barycentric coordinates in primitive
        auto p    = ray.get_position();
        auto bary = detail::gen_barycentric_coords((e_object.transform.affine().inverse() 
                                                    * eig::Vector4f(p.x(), p.y(), p.z(), 1)).head<3>(),
                                                   e_mesh.verts[e_prim[0]],
                                                   e_mesh.verts[e_prim[1]],
                                                   e_mesh.verts[e_prim[2]]);

        fmt::print("{} -> {}\n", p, bary);
      } */
    }
    
  public:
    void init(SchedulerHandle &info) override {
      met_trace();

      m_query_prim   = {{ .cache_handle = info.global("cache") }};
      m_query_result = RayRecord::invalid();
    }
    
    void eval(SchedulerHandle &info) override {
      met_trace();

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

      // If window is not hovered, skip instead of enabling user input over the active viewport
      if (ImGui::IsItemHovered()) {
        // On 'T' press for now, start ray query
        if (ImGui::IsKeyPressed(ImGuiKey_T, false))
          eval_ray_query(info);
      }

      // Iterate all upliftings
      for (const auto &[e_uplifting, _] : e_scene.components.upliftings) {
        // Iterate a copy of all direct surface constraints
        /* for (DirectSurfaceConstraint constraint : e_uplifting.verts | variant_filter<DirectSurfaceConstraint>) {
          
        } */
      }
      

      // Draw current surface constraints using ImGui's drawlist
      {
        for (const auto &[e_uplifting, _] : e_scene.components.upliftings) {
          for (const auto &vert : e_uplifting.verts) {
            if (auto *constraint = std::get_if<DirectSurfaceConstraint>(&vert.constraint)) {
              auto xy = eig::world_to_window_space(constraint->surface_p,
                                                  e_arcball.full(),
                                                  viewport_offs,
                                                  viewport_size);
              if (constraint->is_valid()) {
                ImGui::GetWindowDrawList()->AddCircleFilled(xy, 8.f, ImGui::ColorConvertFloat4ToU32({ .5f, .5f, 1.f, 1.f }));
                ImGui::GetWindowDrawList()->AddCircleFilled(xy, 4.f, ImGui::ColorConvertFloat4ToU32({ 1.f, 1.f, 1.f, 1.f }));
              } else {
                ImGui::GetWindowDrawList()->AddCircleFilled(xy, 8.f, ImGui::ColorConvertFloat4ToU32({ 1.f, .5f, .5f, 1.f }));
                ImGui::GetWindowDrawList()->AddCircleFilled(xy, 4.f, ImGui::ColorConvertFloat4ToU32({ 1.f, 1.f, 1.f, 1.f }));
              }
            }
          }
        }
      }

      // If a valid result is stored, show test tooltip for now
      if (m_query_result.record.is_valid()) {
        // Compute viewport offset and size, minus ImGui's tab bars etc
        eig::Array2f viewport_offs = static_cast<eig::Array2f>(ImGui::GetWindowPos()) 
                                   + static_cast<eig::Array2f>(ImGui::GetWindowContentRegionMin());
        eig::Array2f viewport_size = static_cast<eig::Array2f>(ImGui::GetWindowContentRegionMax())
                                   - static_cast<eig::Array2f>(ImGui::GetWindowContentRegionMin());

        // Get pixel position from query result's world position
        auto xy = eig::world_to_window_space(m_query_result.get_position(),
                                             e_arcball.full(),
                                             viewport_offs,
                                             viewport_size);

        // Spawn floating dot if pixel position falls within viewport
        if (!(xy.array() <= viewport_offs).any() && !(xy.array() >= viewport_offs + viewport_size).any()) {
          
          ImGui::GetWindowDrawList()->AddCircleFilled(xy, 8.f, ImGui::ColorConvertFloat4ToU32({ .5f, .5f, 1.f, 1.f }));
          ImGui::GetWindowDrawList()->AddCircleFilled(xy, 4.f, ImGui::ColorConvertFloat4ToU32({ 1.f, 1.f, 1.f, 1.f }));

          /* ImGui::SetNextWindowPos(xy);
          if (ImGui::Begin("Overlay", nullptr, ImGuiWindowFlags_NoDecoration)) {
            ImGui::Text("Hi!\n");

            // Track mouse input while dragging the thing.
            if (io.MouseDown[ImGuiMouseButton_Left]) {
              eval_ray_query(info);
            }
          }
          ImGui::End(); */
        }
      }
    }
  };
} // namespace met