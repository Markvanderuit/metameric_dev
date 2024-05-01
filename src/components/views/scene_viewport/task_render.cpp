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
  bool MeshViewportRenderTask::is_active(SchedulerHandle &info) {
    met_trace();
    const auto &e_scene = info.global("scene").getr<Scene>();
    guard(!e_scene.components.objects.empty(), false); 
    return info.parent()("is_active").getr<bool>();
  }

  void MeshViewportRenderTask::init(SchedulerHandle &info) {
    met_trace_full(); 
    info("active").set<bool>(true);   
    info("sensor").set<Sensor>({ /* ... */ }).getw<Sensor>().flush();
    info("renderer").init<PathRenderPrimitive>({ .spp_per_iter       = 1u,
                                                 .max_depth          = 4u,
                                                 .pixel_checkerboard = true,
                                                 .cache_handle       = info.global("cache") });
  }
    
  void MeshViewportRenderTask::eval(SchedulerHandle &info) {
    met_trace_full();

    // Get handles, shared resources, modified resources, shorthands
    auto target_handle  = info.relative("viewport_image")("lrgb_target");
    auto camera_handle  = info.relative("viewport_input_camera")("arcball");
    auto render_handle  = info("renderer");
    // auto gbuffer_handle = info("gbuffer");
    auto sensor_handle  = info("sensor");
    const auto &e_scene = info.global("scene").getr<Scene>();
    // const auto &i_pathr = render_handle.getr<GBufferViewPrimitive>();
    const auto &i_pathr = render_handle.getr<PathRenderPrimitive>();

    // Test if renderer necessitates a reset; scene changes, camera changes, target changes
    bool reset_target = target_handle.is_mutated();
    bool reset_camera = camera_handle.is_mutated();
    bool reset = is_first_eval() 
      || info("active").is_mutated()
      || reset_target || reset_camera || e_scene.components;

    // Test if renderer is allowed to render
    guard(info("active").getr<bool>());
    
    // Push sensor changes, reset render component...
    if (reset) {
      const auto &e_target = target_handle.getr<gl::Texture2d4f>();
      const auto &e_camera = camera_handle.getr<detail::Arcball>();
      auto &i_sensor       = sensor_handle.getw<Sensor>();
      auto &i_pathr        = render_handle.getw<PathRenderPrimitive>();
      // auto &i_gbuffer      = gbuffer_handle.getw<PathRenderPrimitive>();

      // auto &i_pathr        = render_handle.getw<GBufferPrimitive>();
      /* auto &i_pathr        = render_handle.getw<PathRenderPrimitive>(); */
      
      float scale = 0.5; // quarter res
      i_sensor.film_size = (e_target.size().cast<float>() * scale).cast<uint>().eval();
      i_sensor.proj_trf  = e_camera.proj().matrix();
      i_sensor.view_trf  = e_camera.view().matrix();
      i_sensor.flush();
      i_pathr.reset(i_sensor, e_scene);

      // Build new gbuffer for hacky denoising
      // i_gbuffer.render(i_sensor, e_scene);
      // i_pathr.render(i_gbuffer, i_sensor, e_scene);
    }

    // ... then call renderer
    if (i_pathr.has_next_sample_state()) {
      auto &i_pathr = render_handle.getw<PathRenderPrimitive>();
      i_pathr.render(sensor_handle.getr<Sensor>(), e_scene);
    }
  }
} // namespace met