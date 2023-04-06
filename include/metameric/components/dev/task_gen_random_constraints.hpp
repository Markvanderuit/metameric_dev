#pragma once

#include <metameric/core/scheduler.hpp>

namespace met {
  struct GenRandomConstraintsTask : public detail::TaskNode {
    bool m_has_run_once;
    
    void init(SchedulerHandle &) override;
    bool is_active(SchedulerHandle &) override;
    void eval(SchedulerHandle &) override;
  };
} // namespace met