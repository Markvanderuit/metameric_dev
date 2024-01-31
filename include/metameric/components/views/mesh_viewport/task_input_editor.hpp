#pragma once

#include <metameric/core/scheduler.hpp>
#include <metameric/core/scene.hpp>
#include <metameric/core/utility.hpp>
#include <metameric/render/sensor.hpp>
#include <small_gl/texture.hpp>

namespace met {
  class MeshViewportEditorInputTask : public detail::TaskNode {
    RaySensor   m_query_sensor;
    PixelSensor m_sensor;

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
      
      // Prepare sensor buffer
      m_sensor.proj_trf  = e_arcball.proj().matrix();
      m_sensor.view_trf  = e_arcball.view().matrix();
      m_sensor.film_size = viewport_size.cast<uint>();
      m_sensor.pixel     = eig::window_to_screen_space(io.MousePos, viewport_offs, viewport_size).cast<uint>();
      m_sensor.flush();

      /* // Generate a camera ray from the current mouse position
      auto screen_pos = eig::window_to_screen_space(io.MousePos, viewport_offs, viewport_size);
      auto camera_ray = e_arcball.generate_ray(screen_pos); */
      
      /* // Prepare sensor buffer
      m_query_sensor.origin    = camera_ray.o;
      m_query_sensor.direction = camera_ray.d;
      m_query_sensor.n_samples = 1;
      m_query_sensor.flush(); */

      // Run raycast primitive, block for results
      // ...
    }
    
  public:
    void init(SchedulerHandle &info) override {
      met_trace();


    }
    
    void eval(SchedulerHandle &info) override {
      met_trace();

      // If window is not hovered, exit now instead of handling user input
      guard(ImGui::IsItemHovered());

      /* // Get handles, shared resources, modified resources
      const auto &e_scene   = info.global("scene").getr<Scene>();
      const auto &e_arcball = info.relative("viewport_input_camera")("arcball").getr<detail::Arcball>();
      const auto &io        = ImGui::GetIO(); */

      // On 'R' press, start ray query
      if (ImGui::IsKeyPressed(ImGuiKey_R, false))
        eval_ray_query(info);
    }
  };
} // namespace met