#include <metameric/core/utility.hpp>
#include <metameric/core/detail/trace.hpp>
#include <metameric/components/views/task_viewport.hpp>
#include <metameric/components/views/viewport/task_viewport_begin.hpp>
#include <metameric/components/views/viewport/task_viewport_overlay.hpp>
#include <metameric/components/views/viewport/task_viewport_input.hpp>
#include <metameric/components/views/viewport/task_viewport_end.hpp>
#include <metameric/components/views/viewport/task_draw_begin.hpp>
// #include <metameric/components/views/viewport/task_draw_gamut.hpp>
#include <metameric/components/views/viewport/task_draw_delaunay.hpp>
#include <metameric/components/views/viewport/task_draw_texture.hpp>
#include <metameric/components/views/viewport/task_draw_csys_ocs.hpp>
#include <metameric/components/views/viewport/task_draw_end.hpp>

namespace met {
  constexpr auto begin_name   = "begin";
  constexpr auto input_name   = "input";
  constexpr auto overlay_name = "overlay";
  constexpr auto end_name     = "end";
  constexpr auto draw_begin_name       = "draw_begin";
  constexpr auto draw_gamut_name       = "draw_gamut";
  constexpr auto draw_delaunay_name    = "draw_delaunay";
  constexpr auto draw_csys_ocs_name    = "draw_csys_ocs";
  constexpr auto draw_texture_name     = "draw_texture";
  constexpr auto draw_cube_name        = "draw_cube";
  constexpr auto draw_end_name         = "draw_end";

  void ViewportTask::init(detail::SchedulerHandle &info) {
    met_trace_full();

    // Add subtasks
    const auto &prnt_key = info.task_key();
    info.emplace_subtask<ViewportDrawEndTask>(prnt_key,      draw_end_name);
    info.emplace_subtask<ViewportDrawTextureTask>(prnt_key,  draw_texture_name);
    info.emplace_subtask<ViewportDrawDelaunayTask>(prnt_key, draw_delaunay_name);
    info.emplace_subtask<ViewportDrawCSysOCSTask>(prnt_key,  draw_csys_ocs_name);
    info.emplace_subtask<ViewportDrawBeginTask>(prnt_key,    draw_begin_name);
    info.emplace_subtask<ViewportEndTask>(prnt_key,          end_name);
    info.emplace_subtask<ViewportInputTask>(prnt_key,        input_name);
    info.emplace_subtask<ViewportOverlayTask>(prnt_key,      overlay_name);
    info.emplace_subtask<ViewportBeginTask>(prnt_key,        begin_name);
  }

  void ViewportTask::dstr(detail::SchedulerHandle &info) {
    met_trace_full();
    
    // Remove subtasks
    const auto &prnt_key = info.task_key();
    info.remove_subtask(prnt_key, begin_name);
    info.remove_subtask(prnt_key, input_name);
    info.remove_subtask(prnt_key, overlay_name);
    info.remove_subtask(prnt_key, end_name);
    info.remove_subtask(prnt_key, draw_begin_name);
    info.remove_subtask(prnt_key, draw_csys_ocs_name);
    info.remove_subtask(prnt_key, draw_delaunay_name);
    info.remove_subtask(prnt_key, draw_texture_name);
    info.remove_subtask(prnt_key, draw_end_name);
  }

  void ViewportTask::eval(detail::SchedulerHandle &info) {
    met_trace_full();
  }
} // namespace met