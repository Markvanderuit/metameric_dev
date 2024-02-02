#pragma once

#include <metameric/core/scheduler.hpp>
#include <metameric/core/ray.hpp>
#include <metameric/core/scene.hpp>
#include <metameric/core/utility.hpp>
#include <metameric/components/views/detail/arcball.hpp>
#include <metameric/components/views/detail/imgui.hpp>
#include <metameric/render/primitives_query.hpp>
#include <small_gl/texture.hpp>
#include <small_gl/window.hpp>

namespace met {
  class MeshViewportCameraInputTask : public detail::TaskNode {
    PixelSensor m_query_sensor;
    uint        m_query_spp = 1;

  public:
    void init(SchedulerHandle &info) override {
      met_trace();

      info("arcball").init<detail::Arcball>({ 
        .dist            = 2.f,
        .e_eye           = { -.5f, .5f, 1.f },
        .e_center        = { -.5f, .5f, .0f },
        .zoom_delta_mult = 0.1f
      });

      info("path_query").init<FullPathQueryPrimitive>({
        .max_depth    = 4,
        .cache_handle = info.global("cache")
      });
    } 

    /* // TODO remove after experimentation
    void eval_rt(SchedulerHandle &info) {
      met_trace();

      // Get shared resources
      const auto &e_scene   = info.global("scene").getr<Scene>();
      const auto &e_objects = e_scene.components.objects;
      const auto &e_meshes  = e_scene.resources.meshes;
      const auto &e_images  = e_scene.resources.images;
      const auto &i_arcball = info.resource("arcball").getr<detail::Arcball>();
      const auto &io        = ImGui::GetIO();

      // Compute viewport offset and size, minus ImGui's tab bars etc
      eig::Array2f viewport_offs = static_cast<eig::Array2f>(ImGui::GetWindowPos()) 
                                 + static_cast<eig::Array2f>(ImGui::GetWindowContentRegionMin());
      eig::Array2f viewport_size = static_cast<eig::Array2f>(ImGui::GetWindowContentRegionMax())
                                 - static_cast<eig::Array2f>(ImGui::GetWindowContentRegionMin());

      // Generate a camera ray from the current mouse position
      auto screen_pos = eig::window_to_screen_space(io.MousePos, viewport_offs, viewport_size);
      auto camera_ray = i_arcball.generate_ray(screen_pos);
      
      // Raytrace to nearest position
      RayQuery object_query;
      uint     object_i;
      for (uint i = 0; i < e_objects.size(); ++i) {
        const auto &object = e_objects[i].value;
        const auto &mesh   = e_meshes[object.mesh_i].value();
        if (auto query = raytrace_elem(camera_ray, mesh);
            query && query.t > 0 && query.t < object_query.t) {
          object_query = query;
          object_i     = i;
        }
      }
      
      if (object_query) {
        const auto &object = e_objects[object_i].value;
        const auto &mesh   = e_meshes[object.mesh_i].value();
        const auto &elem   = mesh.elems[object_query.i];

        // Hit position
        eig::Vector3f p = camera_ray.o + object_query.t * camera_ray.d;

        // Determine barycentrics at hit position 
        eig::Vector3f bary;
        {
          eig::Vector3f a = mesh.verts[elem[0]], b = mesh.verts[elem[1]], c = mesh.verts[elem[2]];
          eig::Vector3f ab = b - a, ac = c - a, ap = p - a;

          float d00 = ab.dot(ab);        
          float d01 = ab.dot(ac);        
          float d11 = ac.dot(ac);        
          float d20 = ap.dot(ab);        
          float d21 = ap.dot(ac);     
          float den = d00 * d11 - d01 * d01;    
          
          bary.y() = (d11 * d20 - d01 * d21) / den;
          bary.z() = (d00 * d21 - d01 * d20) / den;
          bary.x() = 1.f - bary.y() - bary.z();
        }

        // Determine UVs at hit position
        eig::Array2f uv = (mesh.txuvs[elem[0]] * bary[0]
                         + mesh.txuvs[elem[1]] * bary[1]
                         + mesh.txuvs[elem[2]] * bary[2])
                         .unaryExpr([](float f) { return std::fmod(f, 1.f); });

        // Sample surface albedo at hit position
        Colr sample;
        if (object.diffuse.index() == 0) {
          sample = std::get<0>(object.diffuse);
        } else {
          const auto &e_image = e_images[std::get<1>(object.diffuse)].value();
          sample = e_image.sample(uv, Image::ColorFormat::eSRGB).head<3>();
        }

        // Draw temporary tooltip at mouse position
        ImGui::BeginTooltip();
        Colr p_colr  = lrgb_to_srgb(p);
        Colr uv_colr = lrgb_to_srgb(Colr { uv.x(), uv.y(), 0 });
        ImGui::ColorEdit3("Position", p_colr.data(), ImGuiColorEditFlags_Float | ImGuiColorEditFlags_HDR);
        ImGui::ColorEdit3("UV", uv_colr.data(), ImGuiColorEditFlags_Float | ImGuiColorEditFlags_HDR);
        ImGui::ColorEdit3("Sample", sample.data(), ImGuiColorEditFlags_Float | ImGuiColorEditFlags_HDR);
        ImGui::EndTooltip();
      }
    } */

    void eval_path_query(SchedulerHandle &info) {
      met_trace_full();

      // Get shared resources
      const auto &e_scene   = info.global("scene").getr<Scene>();
      const auto &io        = ImGui::GetIO();
      const auto &i_arcball = info("arcball").getr<detail::Arcball>();
      auto &i_path_query    = info("path_query").getw<FullPathQueryPrimitive>();

      // Escape for empty scenes
      guard(!e_scene.components.objects.empty());

      // Compute viewport offset and size, minus ImGui's tab bars etc
      eig::Array2f viewport_offs = static_cast<eig::Array2f>(ImGui::GetWindowPos()) 
                                 + static_cast<eig::Array2f>(ImGui::GetWindowContentRegionMin());
      eig::Array2f viewport_size = static_cast<eig::Array2f>(ImGui::GetWindowContentRegionMax())
                                 - static_cast<eig::Array2f>(ImGui::GetWindowContentRegionMin());

      // Update pixel sensor
      m_query_sensor.proj_trf  = i_arcball.proj().matrix();
      m_query_sensor.view_trf  = i_arcball.view().matrix();
      m_query_sensor.film_size = viewport_size.cast<uint>();
      m_query_sensor.pixel     = eig::window_to_pixel(io.MousePos, viewport_offs, viewport_size);
      m_query_sensor.flush();
      
      fmt::print("{} -> {}\n", m_query_sensor.film_size, m_query_sensor.pixel);
    
      // Perform path query
      i_path_query.query(m_query_sensor, e_scene, m_query_spp);

      // Obtain queried paths
      auto paths = i_path_query.data();
      fmt::print("Queried {} paths, found {}\n",
        m_query_spp,
        paths.size());

      if (!paths.empty()) {
        auto path = paths.front();
        for (uint i = 0; i < path.path_depth; ++i) {
          auto vert = path.data[i];
          guard_break(vert.record.is_valid());
          fmt::print("{}{} - {}\n", 
            vert.record.is_object() ? "Object: " : "Emitter: ",
            vert.record.is_object() ? vert.record.object_i() : vert.record.emitter_i(),
            vert.p);
        }
      }
    }

    void eval(SchedulerHandle &info) override {
      met_trace();

      // Get handle to relative task resources and non-active resources
      auto arcball_handle = info("arcball");
      auto begin_handle   = info.relative("viewport_begin");
    
      // Get shared resources 
      const auto &e_target = begin_handle("lrgb_target").getr<gl::Texture2d4f>();
      const auto &io       = ImGui::GetIO();

      // Arcball; handle aspect modification on viewport resize
      auto viewport_size = e_target.size().cast<float>().eval();
      if (begin_handle("lrgb_target").is_mutated()) {
        auto &i_arcball = arcball_handle.getw<detail::Arcball>();
        i_arcball.set_aspect(viewport_size.x() / viewport_size.y());
      }

      // TODO remove
      if (ImGui::Begin("Blahhh")) {
        uint min_v = 1, max_v = 65536;
        ImGui::SliderScalar("Slider", ImGuiDataType_U32, &m_query_spp, &min_v, &max_v);
      }
      ImGui::End();

      // If window is not hovered, exit now instead of handling user input
      guard(ImGui::IsItemHovered());

      // Arcball; handle mouse scroll
      if (io.MouseWheel != 0.f) {
        auto &i_arcball = arcball_handle.getw<detail::Arcball>();
        i_arcball.set_zoom_delta(-io.MouseWheel);
      }

      // Arcball; handle right mouse controll
      if (io.MouseDown[1]) {
        auto &i_arcball = arcball_handle.getw<detail::Arcball>();
        i_arcball.set_ball_delta(eig::Array2f(io.MouseDelta) / viewport_size.array());
      }

      // Arcball; handle middle mouse controll
      if (io.MouseDown[2]) {
        auto &i_arcball = arcball_handle.getw<detail::Arcball>();
        i_arcball.set_move_delta((eig::Array3f() 
          << eig::Array2f(io.MouseDelta.x, io.MouseDelta.y) / viewport_size.array(), 0).finished());
      }

      // if (ImGui::IsKeyPressed(ImGuiKey_R, false)) {
        eval_path_query(info);
      // }
    }
  };
} // namespace met