#pragma once

#include <metameric/core/scheduler.hpp>

namespace met {
  class WindowTask : public detail::AbstractTask {
  public:
    WindowTask(const std::string &name);
    
    void eval(detail::TaskEvalInfo &info) override;
  };
} // namespace met