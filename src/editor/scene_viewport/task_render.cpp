// Copyright (C) 2024 Mark van de Ruit, Delft University of Technology.
// 
// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.
// 
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
// 
// You should have received a copy of the GNU General Public License
// along with this program.  If not, see <https://www.gnu.org/licenses/>.

#include <metameric/scene/scene.hpp>
#include <metameric/editor/scene_viewport/task_render.hpp>
#include <metameric/editor/detail/arcball.hpp>
#include <metameric/editor/detail/imgui.hpp>
#include <metameric/render/primitives_render.hpp>
#include <metameric/render/sensor.hpp>
#include <small_gl/sampler.hpp>
#include <small_gl/texture.hpp>
#include <small_gl/dispatch.hpp>

namespace met {
  constexpr static uint render_spp_per_iter = 1u;

  bool ViewportRenderTask::is_active(SchedulerHandle &info) {
    met_trace();
    const auto &e_scene = info.global("scene").getr<Scene>();
    guard(!e_scene.components.objects.empty(), false); 
    return info.parent()("is_active").getr<bool>();
  }

  void ViewportRenderTask::init(SchedulerHandle &info) {
    met_trace_full(); 

    // Get handles, shared resources, modified resources, shorthands
    const auto &e_scene = info.global("scene").getr<Scene>();

    info("active").set<bool>(true);   
    info("sensor").set<Sensor>({ /* ... */ }).getw<Sensor>().flush();
  }
    
  void ViewportRenderTask::eval(SchedulerHandle &info) {
    met_trace_full();

    // Get handles, shared resources, modified resources, shorthands
    auto target_handle     = info.relative("viewport_image")("lrgb_target");
    auto camera_handle     = info.relative("viewport_input_camera")("arcball");
    auto render_handle     = info("renderer");
    auto sensor_handle     = info("sensor");
    const auto &e_scene    = info.global("scene").getr<Scene>();
    const auto &e_settings = e_scene.components.settings;

    // (Re-)initialize renderer
    if (is_first_eval() || e_settings.state.renderer_type) {
      switch (e_settings->renderer_type) {
        case Settings::RendererType::ePath:
          render_handle.init<PathRenderPrimitive>({ .spp_per_iter        = render_spp_per_iter,
                                                    .pixel_checkerboard  = true,
                                                    .enable_alpha        = false,
                                                    .enable_debug        = false,
                                                    .cache_handle        = info.global("cache") });
          break;
        case Settings::RendererType::eDirect:
          render_handle.init<PathRenderPrimitive>({ .spp_per_iter        = render_spp_per_iter,
                                                    .max_depth           = 2u,
                                                    .pixel_checkerboard  = true,
                                                    .enable_alpha        = false,
                                                    .enable_debug        = false,
                                                    .cache_handle        = info.global("cache") });
          break;
        case Settings::RendererType::eDebug:
          render_handle.init<PathRenderPrimitive>({ .spp_per_iter        = render_spp_per_iter,
                                                    .max_depth           = 2u,
                                                    .pixel_checkerboard  = true,
                                                    .enable_alpha        = false,
                                                    .enable_debug        = true,
                                                    .cache_handle        = info.global("cache") });
          break;
      }
    }

    // Test if renderer necessitates a reset; scene changes, camera changes, target changes
    bool reset_target = target_handle.is_mutated();
    bool reset_camera = camera_handle.is_mutated();
    bool reset_scene  = e_scene.components.emitters || e_scene.components.objects
      || e_scene.components.upliftings || e_scene.components.views || e_scene.components.settings;
    bool reset = is_first_eval()     // Always run on first call of eval()
      || info("active").is_mutated() // Run if active flag is flipped
      || reset_target 
      || reset_camera 
      || reset_scene;

    // Test if renderer is allowed to render
    guard(info("active").getr<bool>());
    
    // Push sensor changes
    if (reset) {
      // Get shared resources
      const auto &e_target = target_handle.getr<gl::Texture2d4f>();
      const auto &e_camera = camera_handle.getr<detail::Arcball>();

      // Push new sensor data
      auto &i_sensor     = sensor_handle.getw<Sensor>();
      i_sensor.film_size = (e_target.size().cast<float>() * e_settings->view_scale).cast<uint>().eval();
      i_sensor.proj_trf  = e_camera.proj().matrix();
      i_sensor.view_trf  = e_camera.view().matrix();
      i_sensor.flush();

      // Forward to underlying type dependent on setting
      render_handle.getw<PathRenderPrimitive>().reset(i_sensor, e_scene);
    }

    // ... then forward to renderer to update frame if sampler is not exhausted
    if (render_handle.getr<detail::IntegrationRenderPrimitive>().has_next_sample_state()) {
      // Forward to underlying type dependent on setting
      render_handle.getw<PathRenderPrimitive>().render(sensor_handle.getr<Sensor>(), e_scene);
    }
  }
} // namespace met