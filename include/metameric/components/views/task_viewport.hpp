#pragma once

#include <metameric/core/scheduler.hpp>

namespace met {
  class ViewportTask : public detail::TaskBase {
  public:
    void init(SchedulerHandle &) override;
  };
} // namespace met