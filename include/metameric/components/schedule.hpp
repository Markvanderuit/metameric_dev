#pragma once

namespace met {
  template <typename Scheduler>
  void submit_schedule_main(Scheduler &scheduler);
  
  template <typename Scheduler>
  void submit_schedule_empty(Scheduler &scheduler);
} // namespace met