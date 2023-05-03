#include <metameric/core/utility.hpp>
#include <metameric/components/views/task_viewport.hpp>
#include <metameric/components/views/viewport/task_viewport_begin.hpp>
#include <metameric/components/views/viewport/task_viewport_overlay.hpp>
#include <metameric/components/views/viewport/task_viewport_input.hpp>
#include <metameric/components/views/viewport/task_viewport_end.hpp>
#include <metameric/components/views/viewport/task_draw_begin.hpp>
#include <metameric/components/views/viewport/task_draw_meshing.hpp>
#include <metameric/components/views/viewport/task_draw_texture.hpp>
#include <metameric/components/views/viewport/task_draw_color_system_solid.hpp>
#include <metameric/components/views/viewport/task_draw_end.hpp>
// #include <metameric/components/views/dev/task_draw_bvh.hpp>

namespace met {
  void ViewportTask::init(SchedulerHandle &info) {
    met_trace();
    info.subtask("begin").init<ViewportBeginTask>();
    info.subtask("overlay").init<ViewportOverlayTask>();
    info.subtask("input").init<ViewportInputTask>();
    info.subtask("end").init<ViewportEndTask>();
    info.subtask("draw_begin").init<ViewportDrawBeginTask>();
    info.subtask("draw_color_system_solid").init<ViewportDrawColorSystemSolid>();
    info.subtask("draw_meshing").init<ViewportDrawMeshingTask>();
    info.subtask("draw_texture").init<ViewportDrawTextureTask>();
    // info.subtask("draw_bvh").init<ViewportDrawBVHTask>();
    info.subtask("draw_end").init<ViewportDrawEndTask>();
  }
} // namespace met