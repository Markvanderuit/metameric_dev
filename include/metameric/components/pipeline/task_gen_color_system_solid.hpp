#pragma once

#include <metameric/core/scheduler.hpp>

namespace met {
  class GenColorSystemSolidTask : public detail::TaskNode {
  public:
    void init(SchedulerHandle &) override;
    void eval(SchedulerHandle &) override;
    bool eval_state(SchedulerHandle &) override;
  };
} // namespace met