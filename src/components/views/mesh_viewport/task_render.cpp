#include <metameric/core/scene.hpp>
#include <metameric/components/views/mesh_viewport/task_render.hpp>
#include <metameric/components/views/detail/arcball.hpp>
#include <metameric/components/views/detail/imgui.hpp>
#include <metameric/render/primitives_render.hpp>
#include <metameric/render/sensor.hpp>
#include <small_gl/sampler.hpp>
#include <small_gl/texture.hpp>
#include <small_gl/dispatch.hpp>

namespace met {
  using RendererType = PathRenderPrimitive;

  constexpr uint n_iters_per_dispatch = 1u;
  constexpr uint n_iters_max          = 4096u; // 65536u;

  bool MeshViewportRenderTask::is_active(SchedulerHandle &info) {
    met_trace();
    const auto &e_scene  = info.global("scene").getr<Scene>();
    auto is_view_present = info.relative("viewport_begin")("is_active").getr<bool>();
    return !e_scene.components.objects.empty() && is_view_present; 
    // return !(e_scene.components.objects.empty() && e_scene.components.emitters.empty()) && is_view_present; 
  }

  void MeshViewportRenderTask::init(SchedulerHandle &info) {
    met_trace_full();
    info("renderer").init<PathRenderPrimitive>({ .spp_per_iter = n_iters_per_dispatch,  
                                                 .spp_max      = n_iters_max,
                                                 .max_depth    = 4 });
    info("sensor").set<Sensor>({}); // packed gbuffer texture
  }
    
  void MeshViewportRenderTask::eval(SchedulerHandle &info) {
    met_trace_full();

    // Get handles, shared resources, modified resources
    const auto &e_scene = info.global("scene").getr<Scene>();
    auto target_handle  = info.relative("viewport_begin")("lrgb_target");
    auto camera_handle  = info.relative("viewport_input")("arcball");
    auto render_handle  = info("renderer");
    auto sensor_handle  = info("sensor");

    // Test if renderer necessitates a reset; scene changes, camera changes, target changes
    bool reset_target = target_handle.is_mutated();
    bool reset_camera = camera_handle.is_mutated();
    bool reset        = reset_target || reset_camera || e_scene.components;    
    
    // Push sensor changes, reset render component...
    if (reset) {
      const auto &e_target = target_handle.getr<gl::Texture2d4f>();
      const auto &e_camera = camera_handle.getr<detail::Arcball>();
      auto &i_sensor       = sensor_handle.getw<Sensor>();
      auto &i_renderer     = render_handle.getw<RendererType>();
      
      i_sensor.film_size = e_target.size() / 2;
      i_sensor.proj_trf  = e_camera.proj().matrix();
      i_sensor.view_trf  = e_camera.view().matrix();
      i_sensor.flush();

      i_renderer.reset(i_sensor, e_scene);
    }

    // ... then offload rendering
    if (render_handle.getr<RendererType>().has_next_sample_state()) {
      render_handle.getw<RendererType>().render(sensor_handle.getr<Sensor>(), e_scene);
    }
  }
} // namespace met