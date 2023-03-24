#pragma once

#include <metameric/core/scheduler.hpp>

namespace met {
  class ViewportTask : public detail::TaskNode {
  public:
    void init(SchedulerHandle &) override;
  };
} // namespace met