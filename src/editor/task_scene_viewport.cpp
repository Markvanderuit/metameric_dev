#include <metameric/scene/scene.hpp>
#include <metameric/editor/task_scene_viewport.hpp>
#include <metameric/editor/scene_viewport/task_input_editor.hpp>
#include <metameric/editor/scene_viewport/task_render.hpp>
#include <metameric/editor/scene_viewport/task_overlay.hpp>
#include <metameric/editor/scene_viewport/task_combine.hpp>
#include <metameric/editor/detail/arcball.hpp>
#include <metameric/editor/detail/imgui.hpp>
#include <metameric/editor/detail/task_viewport.hpp>
#include <metameric/editor/detail/task_arcball_input.hpp>
#include <metameric/editor/detail/task_lambda.hpp>

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
    info.child_task("viewport_data_connection").init<detail::LambdaTask>([this, arcball_info](SchedulerHandle &info) {
      met_trace();

      // Get shared resources
      const auto &e_scene    = info.global("scene").getr<Scene>();
      const auto &e_settings = e_scene.components.settings;
      const auto &e_view     = e_scene.components.views[e_settings->view_i];
      
      // If the view settings were edited, we reset the arcball to the view's data, but override
      // the specified aspect with the viewport aspect
      if (is_first_eval() || e_settings.state.view_i || e_view) {
        // Get handle to lrgb target
        const auto &lrgb = info.relative("viewport_image")("lrgb_target").getr<gl::Texture2d4f>();
        
        // Initiate arcball from view; override aspect ratio with viewport size
        detail::Arcball arcball = { arcball_info, *e_view };
        arcball.set_aspect(static_cast<float>(lrgb.size().x()) / static_cast<float>(lrgb.size().y()));
        
        info.relative("viewport_input_camera")("arcball").set<detail::Arcball>(std::move(arcball));
      }
    });

    // Subtasks opens a viewport and creates lrgb/srgb image targets; the srgb target
    // is shown in the viewport
    info.child_task("viewport_begin").init<detail::ViewportBeginTask>(viewport_info);
    info.child_task("viewport_image").init<detail::ViewportImageTask>(viewport_info);

    // Get handle to lrgb target
    auto lrgb_target = info.child("viewport_image")("lrgb_target");

    // Subtasks handle arcball camera and user input
    info.child_task("viewport_input_camera").init<detail::ArcballInputTask>(lrgb_target, arcball_info);
    info.child_task("viewport_input_editor").init<ViewportEditorInputTask>();

    // Subtask spawns and manages render primitive 
    info.child_task("viewport_render").init<ViewportRenderTask>();

    // Subtask draws several overlays; uplifting constraints, camera frustra, light paths, etc 
    info.child_task("viewport_draw_overlay").init<ViewportOverlayTask>();

    // Subtask draws render output and overlays into lrgb image target
    info.child_task("viewport_combine").init<ViewportCombineTask>();

    // Subtask copies from lrgb to srgb image target, and closes the viewport
    info.child_task("viewport_end").init<detail::ViewportEndTask>(viewport_info);
  }
}