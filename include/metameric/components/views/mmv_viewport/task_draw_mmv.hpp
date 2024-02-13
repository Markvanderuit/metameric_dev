#pragma once

#include <metameric/core/scheduler.hpp>

namespace met {
  struct DrawMMVTask : public detail::TaskNode  {

  public:
    void init(SchedulerHandle &info) override;
    void eval(SchedulerHandle &info) override;
    bool is_active(SchedulerHandle &info) override;
  };
} // namespace met