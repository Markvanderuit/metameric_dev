#include <metameric/core/scene.hpp>
#include <metameric/components/views/task_scene_viewport.hpp>
#include <metameric/components/views/scene_viewport/task_input_query.hpp>
#include <metameric/components/views/scene_viewport/task_input_editor.hpp>
#include <metameric/components/views/scene_viewport/task_render.hpp>
#include <metameric/components/views/scene_viewport/task_draw_overlay.hpp>
#include <metameric/components/views/scene_viewport/task_draw_combine.hpp>
#include <metameric/components/views/detail/imgui.hpp>
#include <metameric/components/views/detail/task_viewport.hpp>
#include <metameric/components/views/detail/task_arcball_input.hpp>
#include <small_gl/buffer.hpp>
#include <small_gl/dispatch.hpp>
#include <small_gl/framebuffer.hpp>
#include <small_gl/renderbuffer.hpp>
#include <small_gl/program.hpp>
#include <small_gl/sampler.hpp>
#include <small_gl/texture.hpp>
#include <small_gl/utility.hpp>

namespace met {
  void SceneViewportTask::init(SchedulerHandle &info) {
    met_trace();

    // Make is_active available
    info("is_active").set(true);

    detail::ViewportTaskInfo viewport_info = {
      .name         = "Scene viewport",
      .is_closeable = false
    };
    
    // Spawn subtasks
    info.child_task("viewport_begin").init<detail::_ViewportBeginTask>(viewport_info);
    info.child_task("viewport_image").init<detail::_ViewportImageTask>(viewport_info);
    info.child_task("viewport_input_camera").init<detail::ArcballInputTask>(
      info.child("viewport_image")("lrgb_target"));
    info.child_task("viewport_input_editor").init<MeshViewportEditorInputTask>();
    info.child_task("viewport_input_query").init<MeshViewportQueryInputTask>();
    info.child_task("viewport_render").init<MeshViewportRenderTask>();
    info.child_task("viewport_draw_overlay").init<MeshViewportDrawOverlayTask>();
    info.child_task("viewport_draw_combine").init<MeshViewportDrawCombineTask>();
    info.child_task("viewport_end").init<detail::_ViewportEndTask>(viewport_info);
  }

  void SceneViewportTask::eval(SchedulerHandle &info) {
    met_trace();
    // ...
  }
}