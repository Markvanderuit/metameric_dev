#include <metameric/scene/scene.hpp>
#include <metameric/components/views/task_scene_viewport.hpp>
#include <metameric/components/views/scene_viewport/task_input_editor.hpp>
#include <metameric/components/views/scene_viewport/task_render.hpp>
#include <metameric/components/views/scene_viewport/task_draw_overlay.hpp>
#include <metameric/components/views/scene_viewport/task_draw_combine.hpp>
#include <metameric/components/views/detail/arcball.hpp>
#include <metameric/components/views/detail/imgui.hpp>
#include <metameric/components/views/detail/task_viewport.hpp>
#include <metameric/components/views/detail/task_arcball_input.hpp>
#include <metameric/components/misc/task_lambda.hpp>

namespace met {
  void SceneViewportTask::init(SchedulerHandle &info) {
    met_trace();

    // Make is_active available; child tasks can check this flag to know if the viewport
    // is visible, drawable, etc. This flag is expected and set by ViewportBeginTask
    info("is_active").set(true);

    // Specify viewport settings
    detail::ViewportTaskInfo viewport_info = {
      .name         = "Scene viewport",
      .is_closeable = false
    };

    // Specify initial camera settings; these are overridden by the selected scene View object
    detail::ArcballInfo arcball_info = {
      .dist            = 1,
      .e_eye           = { 0,   0,    1 },
      .e_center        = { -0.5, 0.5, 0 },
      .zoom_delta_mult = 0.1f
    };

    // Subtask handles view -> arcball data input before anything else is done; essentially, we
    // reset the viewport camera to the View object only when the user edits said object
    info.child_task("viewport_data_connection").init<LambdaTask>([this, arcball_info](SchedulerHandle &info) {
      met_trace();

      // Get shared resources
      const auto &e_scene    = info.global("scene").getr<Scene>();
      const auto &e_settings = e_scene.components.settings;
      const auto &e_view     = e_scene.components.views[e_settings->view_i];
      
      // If the view settings were edited, we reset the arcball to the view's data
      if (is_first_eval() || e_settings.state.view_i || e_view)
        info.relative("viewport_input_camera")("arcball").set<detail::Arcball>({ arcball_info, *e_view });
    });

    // Subtasks opens a viewport and creates lrgb/srgb image targets; the srgb target
    // is shown in the viewport
    info.child_task("viewport_begin").init<detail::ViewportBeginTask>(viewport_info);
    info.child_task("viewport_image").init<detail::ViewportImageTask>(viewport_info);

    // Get handle to lrgb target
    auto lrgb_target = info.child("viewport_image")("lrgb_target");

    // Subtasks handle arcball camera and other user input
    info.child_task("viewport_input_camera").init<detail::ArcballInputTask>(lrgb_target, arcball_info);
    info.child_task("viewport_input_editor").init<ViewportEditorInputTask>();

    // Subtask spawns and manages render primitive 
    info.child_task("viewport_render").init<MeshViewportRenderTask>();

    // Subtask manages several overlays; uplifting constraints, camera frustrum, etc 
    info.child_task("viewport_draw_overlay").init<ViewportDrawOverlayTask>();

    // Subtask draws render output and overlays into lrgb image target
    info.child_task("viewport_draw_combine").init<ViewportDrawCombineTask>();

    // Subtask copies from lrgb to srgb image target, and closes the viewport
    info.child_task("viewport_end").init<detail::ViewportEndTask>(viewport_info);
  }
}