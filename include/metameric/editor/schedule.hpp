#pragma once

#include <metameric/core/scheduler.hpp>

namespace met {
  void submit_metameric_editor_schedule(detail::SchedulerBase &scheduler);
  void submit_metameric_editor_schedule_unloaded(detail::SchedulerBase &scheduler);
  void submit_metameric_editor_schedule_loaded(detail::SchedulerBase &scheduler);
} // namespace met