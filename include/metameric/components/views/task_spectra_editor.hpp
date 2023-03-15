#pragma once

#include <metameric/core/scheduler.hpp>

namespace met {
  class SpectraEditorTask : public detail::AbstractTask {
  public:
    SpectraEditorTask(const std::string &name);
    void eval(detail::TaskInfo &) override;
  };
} // namespace met
