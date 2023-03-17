#pragma once

#include <metameric/core/scheduler.hpp>
#include <vector>

namespace met {
  class ViewportTask : public detail::TaskBase {
  public:
    void init(detail::SchedulerHandle &) override;
    void dstr(detail::SchedulerHandle &) override;
    void eval(detail::SchedulerHandle &) override;
  };
} // namespace met