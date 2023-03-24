#pragma once

#include <metameric/core/scheduler.hpp>

namespace met {
  class SpectraEditorTask : public detail::TaskNode {
  public:
    void eval(SchedulerHandle &) override;
  };
} // namespace met
