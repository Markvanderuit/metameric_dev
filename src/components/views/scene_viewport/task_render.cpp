#include <metameric/core/scene.hpp>
#include <metameric/components/views/scene_viewport/task_render.hpp>
#include <metameric/components/views/detail/arcball.hpp>
#include <metameric/components/views/detail/imgui.hpp>
#include <metameric/render/primitives_render.hpp>
#include <metameric/render/sensor.hpp>
#include <small_gl/sampler.hpp>
#include <small_gl/texture.hpp>
#include <small_gl/dispatch.hpp>

namespace met {
  constexpr static uint render_spp_per_iter = 1u;

  bool MeshViewportRenderTask::is_active(SchedulerHandle &info) {
    met_trace();
    const auto &e_scene = info.global("scene").getr<Scene>();
    guard(!e_scene.components.objects.empty(), false); 
    return info.parent()("is_active").getr<bool>();
  }

  void MeshViewportRenderTask::init(SchedulerHandle &info) {
    met_trace_full(); 

    // Get handles, shared resources, modified resources, shorthands
    const auto &e_scene = info.global("scene").getr<Scene>();

    info("active").set<bool>(true);   
    info("sensor").set<Sensor>({ /* ... */ }).getw<Sensor>().flush();
  }
    
  void MeshViewportRenderTask::eval(SchedulerHandle &info) {
    met_trace_full();

    // Get handles, shared resources, modified resources, shorthands
    auto target_handle     = info.relative("viewport_image")("lrgb_target");
    auto camera_handle     = info.relative("viewport_input_camera")("arcball");
    auto render_handle     = info("renderer");
    auto sensor_handle     = info("sensor");
    const auto &e_scene    = info.global("scene").getr<Scene>();
    const auto &e_view_i   = info.parent()("view_settings_i").getr<uint>();
    const auto &e_view     = e_scene.components.views[e_view_i].value;
    const auto &e_settings = e_scene.components.settings.value;

    // (Re-)initialize renderer
    if (is_first_eval() || e_scene.components.settings.state.renderer_type) {
      switch (e_settings.renderer_type) {
        case Settings::RendererType::ePath:
          render_handle.init<PathRenderPrimitive>({ .spp_per_iter        = render_spp_per_iter,
                                                    .pixel_checkerboard  = true,
                                                    .enable_alpha        = true,
                                                    .cache_handle        = info.global("cache") });
          break;
        case Settings::RendererType::eDirect:
          render_handle.init<PathRenderPrimitive>({ .spp_per_iter        = render_spp_per_iter,
                                                    .max_depth           = 2u,
                                                    .pixel_checkerboard  = true,
                                                    .cache_handle        = info.global("cache") });
          break;
        case Settings::RendererType::eDebug:
          render_handle.init<PathRenderPrimitive>({ .spp_per_iter        = render_spp_per_iter,
                                                    .max_depth           = 2u,
                                                    .pixel_checkerboard  = true,
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
    
    // Push sensor changes, reset render component...
    if (reset) {
      // Get shared resources
      const auto &e_target = target_handle.getr<gl::Texture2d4f>();
      const auto &e_camera = camera_handle.getr<detail::Arcball>();

      // Push new sensor data
      auto &i_sensor     = sensor_handle.getw<Sensor>();
      i_sensor.film_size = (e_target.size().cast<float>() * e_settings.view_scale).cast<uint>().eval();
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