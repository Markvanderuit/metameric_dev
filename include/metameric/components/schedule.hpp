#pragma once

#include <metameric/core/scheduler.hpp>

namespace met {
  void submit_schedule_main(detail::SchedulerBase &scheduler);
  void submit_schedule_empty(detail::SchedulerBase &scheduler);
} // namespace met