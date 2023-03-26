#pragma once

#include <metameric/core/math.hpp>
#include <metameric/core/scheduler.hpp>

namespace met {
  class GenColorSystemsTask : public detail::TaskNode {
    uint m_max_maps;
    
  public:
    void init(SchedulerHandle &) override;
    void eval(SchedulerHandle &) override;
    bool is_active(SchedulerHandle &) override;
  };
} // namespace met