#pragma once

#include <metameric/core/scheduler.hpp>

namespace met {
  class SceneHandlerTask : public detail::TaskNode {
    bool m_is_init = false;

  public:
    void init(SchedulerHandle &info) override;
    void eval(SchedulerHandle &info) override;
  };
} // namespace met