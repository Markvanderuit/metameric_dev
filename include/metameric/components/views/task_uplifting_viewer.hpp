#pragma once

#include <metameric/core/scheduler.hpp>

namespace met {
  struct UpliftingViewerTask : public detail::TaskNode  {
    uint m_uplifting_i;

  public:
    UpliftingViewerTask(uint uplifting_i) 
    : m_uplifting_i(uplifting_i) { }

    void init(SchedulerHandle &info) override;
    void eval(SchedulerHandle &info) override;
    bool is_active(SchedulerHandle &info) override;
  };
} // namespace met