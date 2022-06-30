#pragma once

#include <metameric/gui/detail/linear_scheduler/task.hpp>

namespace met {
  class WindowTask : public detail::AbstractTask {
  public:
    WindowTask(const std::string &name);
    void init(detail::TaskInitInfo &info) override;
    void eval(detail::TaskEvalInfo &info) override;
  };
} // namespace met