#pragma once

#include <metameric/core/scheduler.hpp>
#include <metameric/components/views/mesh_viewport/task_input.hpp>
#include <metameric/components/views/mesh_viewport/task_render.hpp>
#include <metameric/components/views/mesh_viewport/task_draw_overlay.hpp>
#include <metameric/components/views/mesh_viewport/task_draw_combine.hpp>
#include <metameric/components/views/detail/task_viewport.hpp>

namespace met {
  struct MeshViewportTask : public detail::TaskNode {
    void init(SchedulerHandle &info) override {
      met_trace();

      info.child_task("viewport_begin").init<detail::ViewportBeginTask>();
      info.child_task("viewport_input").init<MeshViewportInputTask>();
      info.child_task("viewport_render").init<MeshViewportRenderTask>();
      info.child_task("viewport_draw_overlay").init<MeshViewportDrawOverlayTask>();
      info.child_task("viewport_draw_combine").init<MeshViewportDrawCombineTask>();
      info.child_task("viewport_end").init<detail::ViewportEndTask>();
    }
  };
} // namespace met