#pragma once

#include <metameric/core/mesh.hpp>
#include <metameric/core/scheduler.hpp>

namespace met {
  class GenColorSolidsTask : public detail::TaskBase {
  public:
    void init(SchedulerHandle &) override;
    void eval(SchedulerHandle &) override;
  };
} // namespace met