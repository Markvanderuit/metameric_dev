#pragma once

#include <metameric/core/math.hpp>
#include <metameric/core/scheduler.hpp>

namespace met {
  class GenColorSystemsTask : public detail::TaskBase {
    uint m_max_maps;
    
  public:
    void init(detail::TaskInfo &) override;
    void eval(detail::TaskInfo &) override;
  };
} // namespace met