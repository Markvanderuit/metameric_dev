#include <metameric/core/utility.hpp>
#include <metameric/components/views/task_viewport.hpp>
#include <metameric/components/views/viewport/task_viewport_begin.hpp>
#include <metameric/components/views/viewport/task_viewport_overlay.hpp>
#include <metameric/components/views/viewport/task_viewport_input.hpp>
#include <metameric/components/views/viewport/task_viewport_end.hpp>
#include <metameric/components/views/viewport/task_draw_begin.hpp>
#include <metameric/components/views/viewport/task_draw_delaunay.hpp>
#include <metameric/components/views/viewport/task_draw_texture.hpp>
#include <metameric/components/views/viewport/task_draw_csys_ocs.hpp>
#include <metameric/components/views/viewport/task_draw_end.hpp>

namespace met {
  void ViewportTask::init(SchedulerHandle &info) {
    met_trace_full();
    info.subtask("begin").init<ViewportBeginTask>();
    info.subtask("overlay").init<ViewportOverlayTask>();
    info.subtask("input").init<ViewportInputTask>();
    info.subtask("end").init<ViewportEndTask>();
    info.subtask("draw_begin").init<ViewportDrawBeginTask>();
    info.subtask("draw_csys_ocs").init<ViewportDrawCSysOCSTask>();
    info.subtask("draw_delaunay").init<ViewportDrawDelaunayTask>();
    info.subtask("draw_texture").init<ViewportDrawTextureTask>();
    info.subtask("draw_end").init<ViewportDrawEndTask>();
  }
} // namespace met