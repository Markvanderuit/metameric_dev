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
  class MeshViewportQueryInputTask : public detail::TaskNode {
    PixelSensor m_query_sensor;
    uint        m_query_spp = 0;

  public:
    void init(SchedulerHandle &info) override {
      met_trace();
      info("path_query").init<FullPathQueryPrimitive>({
        .max_depth    = 4,
        .cache_handle = info.global("cache")
      });
    } 

    void eval_path_query(SchedulerHandle &info) {
      met_trace_full();

      // Get shared resources
      const auto &e_scene   = info.global("scene").getr<Scene>();
      const auto &io        = ImGui::GetIO();
      const auto &e_arcball = info.relative("viewport_input_camera")("arcball").getr<detail::Arcball>();
      auto &i_path_query    = info("path_query").getw<FullPathQueryPrimitive>();

      // Escape for empty scenes
      guard(!e_scene.components.objects.empty());

      // Compute viewport offset and size, minus ImGui's tab bars etc
      eig::Array2f viewport_offs = static_cast<eig::Array2f>(ImGui::GetWindowPos()) 
                                 + static_cast<eig::Array2f>(ImGui::GetWindowContentRegionMin());
      eig::Array2f viewport_size = static_cast<eig::Array2f>(ImGui::GetWindowContentRegionMax())
                                 - static_cast<eig::Array2f>(ImGui::GetWindowContentRegionMin());

      // Update pixel sensor
      m_query_sensor.proj_trf  = e_arcball.proj().matrix();
      m_query_sensor.view_trf  = e_arcball.view().matrix();
      m_query_sensor.film_size = viewport_size.cast<uint>();
      m_query_sensor.pixel     = eig::window_to_pixel(io.MousePos, viewport_offs, viewport_size);
      m_query_sensor.flush();
      
      // fmt::print("{} -> {}\n", m_query_sensor.film_size, m_query_sensor.pixel);
    
      // Perform path query
      i_path_query.query(m_query_sensor, e_scene, m_query_spp);

      // Obtain queried paths
      // auto paths = i_path_query.data();
    }

    void eval(SchedulerHandle &info) override {
      met_trace();
      
      if (ImGui::Begin("Blahhh")) {
        uint min_v = 0, max_v = 65536;
        ImGui::SliderScalar("Slider", ImGuiDataType_U32, &m_query_spp, &min_v, &max_v);
      }
      ImGui::End();
      
      if (m_query_spp > 0)
        eval_path_query(info);
    }
  };
} // namespace met