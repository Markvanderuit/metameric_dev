#pragma once

#include <metameric/core/scheduler.hpp>

namespace met {
  struct SceneHandlerTask : public detail::TaskNode {
    void init(SchedulerHandle &info) override;
    void eval(SchedulerHandle &info) override;
  };
} // namespace met