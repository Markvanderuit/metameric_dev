#include <metameric/core/math.hpp>
#include <metameric/scene/scene.hpp>
#include <metameric/core/scheduler.hpp> 
#include <metameric/core/spectrum.hpp>
#include <metameric/core/ranges.hpp>
#include <metameric/core/utility.hpp>
#include <metameric/editor/schedule.hpp>
#include <metameric/editor/task_window.hpp>
#include <metameric/editor/task_scene_viewport.hpp>
#include <metameric/editor/detail/task_lambda.hpp>
#include <metameric/editor/detail/task_frame_begin.hpp>
#include <metameric/editor/detail/task_frame_end.hpp>
#include <metameric/editor/detail/component_edit.hpp>
#include <metameric/editor/detail/imgui.hpp>

namespace met {
  void submit_editor_schedule_unloaded(detail::SchedulerBase &scheduler) {
    met_trace();

    debug::check_expr(scheduler.global("scene").is_init() && 
                      scheduler.global("window").is_init());
    
    scheduler.clear();
    
    scheduler.task("frame_begin").init<detail::FrameBeginTask>();
    scheduler.task("window").init<WindowTask>();
    scheduler.task("frame_end").init<detail::FrameEndTask>(false);
  }

  void submit_editor_schedule_loaded(detail::SchedulerBase &scheduler) {
    met_trace();

    debug::check_expr(scheduler.global("scene").is_init() && 
                      scheduler.global("window").is_init());
    
    scheduler.clear();
    
    // Initial window/gl tasks
    scheduler.task("frame_begin").init<detail::FrameBeginTask>();
    scheduler.task("window").init<WindowTask>();

    // Boilerplate task which triggers scene state updates, filters some edge cases, and
    // generally keeps everything running nicely.
    scheduler.task("scene_handler").init<detail::LambdaTask>([](auto &info) {
      met_trace();
      info.global("scene").template getw<Scene>().update();
    });

    // Editor task for scene components (objects, emitters, etc)
    scheduler.task("scene_components_editor").init<detail::LambdaTask>([](auto &info) {
      met_trace();
      if (ImGui::Begin("Scene components")) {
        push_editor<detail::Component<Uplifting>>(info, { .editor_name = "Upliftings", .default_open = true });
        push_editor<detail::Component<Object>>(info,    { .editor_name = "Objects"  });
        push_editor<detail::Component<Emitter>>(info,   { .editor_name = "Emitters" });
        push_editor<detail::Component<View>>(info,      { .editor_name = "Views" });
      }
      ImGui::End();
    });

    // Editor task for scene resources (meshes, textures, etc)
    scheduler.task("scene_resource_editor").init<detail::LambdaTask>([](auto &info) {
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

    // Viewport task which handles camera input, renders scene, and draws to a viewport
    scheduler.task("viewport").init<SceneViewportTask>();

    // Final window/gl task
    scheduler.task("frame_end").init<detail::FrameEndTask>(false);
  }
  
  void submit_editor_schedule_auto(detail::SchedulerBase &scheduler) {
    met_trace();

    debug::check_expr(scheduler.global("scene").is_init() && 
                      scheduler.global("window").is_init());

    const auto &e_scene = scheduler.global("scene").getr<Scene>();
    if (e_scene.save_state == Scene::SaveState::eUnloaded) {
      submit_editor_schedule_unloaded(scheduler);
    } else {
      submit_editor_schedule_loaded(scheduler);
    }
  }
} // namespace met