#pragma once

#include <metameric/core/scheduler.hpp>

namespace met {
  void submit_schedule(detail::SchedulerBase &scheduler);
  void submit_schedule_main(detail::SchedulerBase &scheduler);
  void submit_schedule_empty(detail::SchedulerBase &scheduler);

  void submit_metameric_editor_schedule(detail::SchedulerBase &scheduler);
  void submit_metameric_editor_schedule_unloaded(detail::SchedulerBase &scheduler);
  void submit_metameric_editor_schedule_loaded(detail::SchedulerBase &scheduler);
} // namespace met