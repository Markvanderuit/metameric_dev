#pragma once

#include <metameric/core/scheduler.hpp>

namespace met {
  class GenColorSystemSolidTask : public detail::TaskNode {
  public:
    void eval(SchedulerHandle &) override;
    bool is_active(SchedulerHandle &) override;
  };
} // namespace met