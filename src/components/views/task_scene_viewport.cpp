#include <metameric/core/scene.hpp>
#include <metameric/components/views/task_scene_viewport.hpp>
#include <metameric/components/views/scene_viewport/task_input_query.hpp>
#include <metameric/components/views/scene_viewport/task_input_editor.hpp>
#include <metameric/components/views/scene_viewport/task_render.hpp>
#include <metameric/components/views/scene_viewport/task_export.hpp>
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

    // Make is_active available
    info("is_active").set(true);

    // Refer to view settings for the viewport
    info("view_settings_i").set<uint>(0);

    // Specify viewport settings
    detail::ViewportTaskInfo viewport_info = {
      .name         = "Scene viewport",
      .is_closeable = false
    };

    // Spawn subtasks
    info.child_task("viewport_begin").init<detail::_ViewportBeginTask>(viewport_info);
    info.child_task("viewport_image").init<detail::_ViewportImageTask>(viewport_info);
    info.child_task("viewport_input_camera").init<detail::ArcballInputTask>(info.child("viewport_image")("lrgb_target"),
    detail::ArcballInfo {
      .dist            = 1,
      .e_eye           = { 0,   0,    1 },
      .e_center        = { -0.5, 0.5, 0 },
      .zoom_delta_mult = 0.1f
    });
    info.child_task("viewport_input_editor").init<MeshViewportEditorInputTask>();
    info.child_task("viewport_input_query").init<MeshViewportQueryInputTask>();
    info.child_task("viewport_render").init<MeshViewportRenderTask>();
    info.child_task("viewport_draw_overlay").init<MeshViewportDrawOverlayTask>();
    info.child_task("viewport_draw_combine").init<MeshViewportDrawCombineTask>();

    // Subtask to handle viewport scene settings
    info.child_task("viewport_data_connection").init<LambdaTask>([this](SchedulerHandle &info) {
      met_trace();
      const auto &e_scene   = info.global("scene").getr<Scene>();
      const auto &e_view_i  = info.parent()("view_settings_i").getr<uint>();
      auto camera_handle    = info.relative("viewport_input_camera")("arcball");
      const auto &[e_view, 
                   e_state] = e_scene.components.views[e_view_i];

      if (e_state || is_first_eval()) { // View data changed in scene, overwrites arcball for now
        fmt::print("State updated\n");
        
        auto &e_arcball = camera_handle.getw<detail::Arcball>();

        eig::Affine3f trf_rot = eig::Affine3f::Identity();
        trf_rot *= eig::AngleAxisf(e_view.camera_trf.rotation.x(), eig::Vector3f::UnitY());
        trf_rot *= eig::AngleAxisf(e_view.camera_trf.rotation.y(), eig::Vector3f::UnitX());
        trf_rot *= eig::AngleAxisf(e_view.camera_trf.rotation.z(), eig::Vector3f::UnitZ());

        auto dir = (trf_rot * eig::Vector3f(0, 0, 1)).normalized().eval();
        auto eye = -dir; 
        auto cen = (e_view.camera_trf.position + dir).eval();

        e_arcball.set_fov_y(e_view.camera_fov_y * std::numbers::pi_v<float> / 180.f);
        e_arcball.set_zoom(1);
        e_arcball.set_eye(eye);
        e_arcball.set_center(cen);
      }
    });

    info.child_task("viewport_end").init<detail::_ViewportEndTask>(viewport_info);
    
    // TODO move to the right place
    info.child_task("viewport_export").init<MeshViewportExportTask>();
  }

  void SceneViewportTask::eval(SchedulerHandle &info) {
    met_trace();
    // ...
  }
}