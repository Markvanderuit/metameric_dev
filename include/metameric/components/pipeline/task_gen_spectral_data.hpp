#pragma once

#include <metameric/core/scheduler.hpp>

namespace met {
  struct GenSpectralDataTask : public detail::TaskNode {
    void init(SchedulerHandle &) override;
    void eval(SchedulerHandle &) override;
    bool is_active(SchedulerHandle &) override;
  };
} // namespace met