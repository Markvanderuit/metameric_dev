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
    info.emplace_subtask<ViewportBeginTask>("begin");
    info.emplace_subtask<ViewportOverlayTask>("overlay");
    info.emplace_subtask<ViewportInputTask>("input");
    info.emplace_subtask<ViewportEndTask>("end");
    info.emplace_subtask<ViewportDrawBeginTask>("draw_begin");
    info.emplace_subtask<ViewportDrawCSysOCSTask>("draw_csys_ocs");
    info.emplace_subtask<ViewportDrawDelaunayTask>("draw_delaunay");
    info.emplace_subtask<ViewportDrawTextureTask>("draw_texture");
    info.emplace_subtask<ViewportDrawEndTask>("draw_end");
  }
} // namespace met