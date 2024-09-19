#include <metameric/core/math.hpp>
#include <metameric/core/scene.hpp>
#include <metameric/core/scheduler.hpp> 
#include <metameric/core/spectrum.hpp>
#include <metameric/core/ranges.hpp>
#include <metameric/core/utility.hpp>
#include <metameric/components/schedule.hpp>
#include <metameric/components/misc/task_lambda.hpp>
#include <metameric/components/misc/task_frame_begin.hpp>
#include <metameric/components/misc/task_frame_end.hpp>
#include <metameric/components/views/task_window.hpp>
#include <metameric/components/views/task_scene_viewport.hpp>
#include <metameric/components/views/detail/imgui.hpp>
#include <metameric/components/views/detail/component_edit.hpp>
#include <metameric/components/views/detail/task_viewport.hpp>
#include <metameric/components/views/detail/task_arcball_input.hpp>
#include <metameric/components/pipeline/task_gen_uplifting_data.hpp>
#include <metameric/components/pipeline/task_gen_object_data.hpp>

namespace met {
  void submit_metameric_editor_schedule_unloaded(detail::SchedulerBase &scheduler) {
    met_trace();

    debug::check_expr(scheduler.global("scene").is_init() && 
                      scheduler.global("window").is_init());
    
    scheduler.clear();
    
    scheduler.task("frame_begin").init<FrameBeginTask>();
    scheduler.task("window").init<WindowTask>();
    scheduler.task("frame_end").init<FrameEndTask>(false);
  }

  void submit_metameric_editor_schedule_loaded(detail::SchedulerBase &scheduler) {
    met_trace();

    debug::check_expr(scheduler.global("scene").is_init() && 
                      scheduler.global("window").is_init());
    
    scheduler.clear();
    
    // Initial window/gl tasks
    scheduler.task("frame_begin").init<FrameBeginTask>();
    scheduler.task("window").init<WindowTask>();

    // Boilerplate task which triggers scene state updates, filters some edge cases, and
    // generally keeps everything running nicely.
    scheduler.task("scene_handler").init<LambdaTask>([](SchedulerHandle &info) {
      met_trace();

      // Get shared resources
      auto &e_scene = info.global("scene").getw<Scene>();

      // Force check of scene indices to ensure linked components/resources still exist
      for (auto [i, comp] : enumerate_view(e_scene.components.objects)) {
        auto &obj = comp.value;
        if (obj.mesh_i >= e_scene.resources.meshes.size())
          obj.mesh_i = 0u;
        if (obj.uplifting_i >= e_scene.components.upliftings.size())
          obj.uplifting_i = 0u;
      }
      for (auto [i, comp] : enumerate_view(e_scene.components.emitters)) {
        auto &emt = comp.value;
        if (emt.illuminant_i >= e_scene.resources.illuminants.size())
          emt.illuminant_i = 0u;
      }
      for (auto [i, comp] : enumerate_view(e_scene.components.upliftings)) {
        auto &upl = comp.value;
        if (upl.observer_i >= e_scene.resources.observers.size())
          upl.observer_i = 0u;
        if (upl.illuminant_i >= e_scene.resources.illuminants.size())
          upl.illuminant_i = 0u;
      }
      {
        auto &settings = e_scene.components.settings.value;
        if (settings.view_i >= e_scene.components.views.size())
          settings.view_i = 0u;
      }

      // Force update check of stale gl-side components and state tracking
      e_scene.resources.meshes.update(e_scene);
      e_scene.resources.images.update(e_scene);
      e_scene.resources.illuminants.update(e_scene);
      e_scene.resources.observers.update(e_scene);
      e_scene.resources.bases.update(e_scene);
      e_scene.components.settings.state.update(e_scene.components.settings.value);
      e_scene.components.emitters.update(e_scene);
      e_scene.components.objects.update(e_scene);
      e_scene.components.upliftings.update(e_scene);
      e_scene.components.views.update(e_scene);
    });

    // Pipeline tasks generate uplifting data and then bake a spectral texture per object
    scheduler.task("gen_upliftings").init<GenUpliftingsTask>();
    scheduler.task("gen_objects").init<GenObjectsTask>();

    // Editor task for scene components (objects, emitters, etc)
    scheduler.task("scene_components_editor").init<LambdaTask>([](auto &info) {
      met_trace();
      if (ImGui::Begin("Scene components")) {
        push_editor<detail::Component<Object>>(info,       { .editor_name = "Objects" });
        push_editor<detail::Component<Emitter>>(info,      { .editor_name = "Emitters" });
        push_editor<detail::Component<Uplifting>>(info,    { .editor_name = "Uplifting models" });
        push_editor<detail::Component<View>>(info, { .editor_name = "Views" });
      }
      ImGui::End();
    });

    // Editor task for scene resources (meshes, textures, etc)
    scheduler.task("scene_resource_editor").init<LambdaTask>([](auto &info) {
      met_trace();
      if (ImGui::Begin("Scene resources")) {
        push_editor<detail::Resource<Mesh>>(info, { .editor_name  = "Meshes",
                                                    .show_add     = false,
                                                    .show_del     = true,
                                                    .show_dupl    = false });
        push_editor<detail::Resource<Image>>(info, { .editor_name = "Textures",
                                                     .show_add    = false,
                                                     .show_del    = true,
                                                     .show_dupl   = false });
      }
      ImGui::End();
    });

    // Viewport task which takes camera input, renders scene, and shows scene
    scheduler.task("viewport").init<SceneViewportTask>();

    // Final window/gl task
    scheduler.task("frame_end").init<FrameEndTask>(false);
  }
  
  void submit_metameric_editor_schedule(detail::SchedulerBase &scheduler) {
    met_trace();

    debug::check_expr(scheduler.global("scene").is_init() && 
                      scheduler.global("window").is_init());

    const auto &e_scene = scheduler.global("scene").getr<Scene>();
    if (e_scene.save_state == Scene::SaveState::eUnloaded) {
      submit_metameric_editor_schedule_unloaded(scheduler);
    } else {
      submit_metameric_editor_schedule_loaded(scheduler);
    }
  }
} // namespace met