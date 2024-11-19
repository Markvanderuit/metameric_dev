#pragma once

#include <metameric/core/scheduler.hpp>

namespace met {
  // Submit editor tasks to program schedule, s.t. a default empty view is given
  void submit_editor_schedule_unloaded(detail::SchedulerBase &scheduler);

  // Submit editor tasks to program schedule, s.t. a scene editor is shown for the current scene
  void submit_editor_schedule_loaded(detail::SchedulerBase &scheduler);

  // Submit editor tasks to program schedule, dependent on current scene state in scheduler data
  void submit_editor_schedule_auto(detail::SchedulerBase &scheduler);
} // namespace met