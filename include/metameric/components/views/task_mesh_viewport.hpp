#pragma once

#include <metameric/core/scheduler.hpp>

namespace met {
  struct MeshViewportTask : public detail::TaskNode {
    void init(SchedulerHandle &) override;
  };
} // namespace met