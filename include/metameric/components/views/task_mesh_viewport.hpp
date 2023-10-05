#pragma once

#include <metameric/core/scheduler.hpp>
#include <metameric/components/views/mesh_viewport/task_draw.hpp>
#include <metameric/components/views/mesh_viewport/task_input.hpp>
#include <metameric/components/views/mesh_viewport/task_draw_uplifted.hpp>
#include <metameric/components/views/mesh_viewport/task_draw_direct.hpp>
#include <metameric/components/views/mesh_viewport/task_draw_gbuffer.hpp>
#include <metameric/components/views/detail/task_viewport.hpp>

namespace met {
  struct MeshViewportTask : public detail::TaskNode {
    void init(SchedulerHandle &info) override {
      met_trace();

      info.child_task("viewport_begin").init<detail::ViewportBeginTask>();
      info.child_task("viewport_input").init<MeshViewportInputTask>();
      info.child_task("viewport_draw_gbuffer").init<MeshViewportDrawGBufferTask>();
      info.child_task("viewport_draw_direct").init<MeshViewportDrawDirectTask>();
      info.child_task("viewport_end").init<detail::ViewportEndTask>();
    }
  };
} // namespace met