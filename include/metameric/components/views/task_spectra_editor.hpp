#pragma once

#include <metameric/core/scheduler.hpp>

namespace met {
  class SpectraEditorTask : public detail::TaskBase {
  public:
    void eval(detail::SchedulerHandle &) override;
  };
} // namespace met
