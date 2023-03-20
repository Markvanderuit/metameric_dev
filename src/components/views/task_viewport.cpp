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

    // Add subtasks
    const auto &prnt_key = info.task_key();
    info.emplace_subtask<ViewportBeginTask>(prnt_key,        "begin");
    info.emplace_subtask<ViewportOverlayTask>(prnt_key,      "overlay");
    info.emplace_subtask<ViewportInputTask>(prnt_key,        "input");
    info.emplace_subtask<ViewportEndTask>(prnt_key,          "end");
    info.emplace_subtask<ViewportDrawBeginTask>(prnt_key,    "draw_begin");
    info.emplace_subtask<ViewportDrawCSysOCSTask>(prnt_key,  "draw_csys_ocs");
    info.emplace_subtask<ViewportDrawDelaunayTask>(prnt_key, "draw_delaunay");
    info.emplace_subtask<ViewportDrawTextureTask>(prnt_key,  "draw_texture");
    info.emplace_subtask<ViewportDrawEndTask>(prnt_key,      "draw_end");
  }
} // namespace met