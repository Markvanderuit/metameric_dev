#include <metameric/core/utility.hpp>
#include <metameric/core/detail/trace.hpp>
#include <metameric/components/views/task_viewport.hpp>
#include <metameric/components/views/viewport/task_viewport_begin.hpp>
#include <metameric/components/views/viewport/task_viewport_tooltip.hpp>
#include <metameric/components/views/viewport/task_viewport_input.hpp>
#include <metameric/components/views/viewport/task_viewport_end.hpp>
#include <metameric/components/views/viewport/task_draw_begin.hpp>
#include <metameric/components/views/viewport/task_draw_gamut.hpp>
#include <metameric/components/views/viewport/task_draw_cube.hpp>
#include <metameric/components/views/viewport/task_draw_texture.hpp>
#include <metameric/components/views/viewport/task_draw_end.hpp>

namespace met {
  constexpr auto viewport_begin_name   = "_begin";
  constexpr auto viewport_input_name   = "_input";
  constexpr auto viewport_tooltip_name = "_tooltip";
  constexpr auto viewport_end_name     = "_end";
  constexpr auto draw_begin_name       = "_draw_begin";
  constexpr auto draw_gamut_name       = "_draw_gamut";
  constexpr auto draw_texture_name     = "_draw_texture";
  constexpr auto draw_cube_name        = "_draw_cube";
  constexpr auto draw_end_name         = "_draw_end";
  
  ViewportTask::ViewportTask(const std::string &name)
  : detail::AbstractTask(name) { }

  void ViewportTask::init(detail::TaskInitInfo &info) {
    met_trace_full();

    // Add drawing subtasks in reverse order
    info.emplace_task_after<ViewportDrawEndTask>(name(),      name() + draw_end_name);
    info.emplace_task_after<ViewportDrawCubeTask>(name(),     name() + draw_cube_name);
    info.emplace_task_after<ViewportDrawGamutTask>(name(),    name() + draw_gamut_name);
    info.emplace_task_after<ViewportDrawTextureTask>(name(),  name() + draw_texture_name);
    info.emplace_task_after<ViewportDrawBeginTask>(name(),    name() + draw_begin_name);

    // Add UI subtasks in reverse order
    info.emplace_task_after<ViewportEndTask>(name(),          name() + viewport_end_name);
    info.emplace_task_after<ViewportTooltipTask>(name(),      name() + viewport_tooltip_name);
    info.emplace_task_after<ViewportInputTask>(name(),        name() + viewport_input_name);
    info.emplace_task_after<ViewportBeginTask>(name(),        name() + viewport_begin_name);
  }

  void ViewportTask::dstr(detail::TaskDstrInfo &info) {
    met_trace_full();
    
    // Remove subtasks
    info.remove_task(name() + viewport_begin_name);
    info.remove_task(name() + viewport_input_name);
    info.remove_task(name() + viewport_tooltip_name);
    info.remove_task(name() + viewport_end_name);
    info.remove_task(name() + draw_begin_name);
    info.remove_task(name() + draw_texture_name);
    info.remove_task(name() + draw_gamut_name);
    info.remove_task(name() + draw_cube_name);
    info.remove_task(name() + draw_end_name);
  }

  void ViewportTask::eval(detail::TaskEvalInfo &info) {
    met_trace_full();
  }
} // namespace met