#pragma once

#include <metameric/core/scheduler.hpp>

namespace met {
  struct GenRandomConstraintsTask : public detail::TaskNode {
    void init(SchedulerHandle &) override;
    bool is_active(SchedulerHandle &) override;
    void eval(SchedulerHandle &) override;
  };
} // namespace met