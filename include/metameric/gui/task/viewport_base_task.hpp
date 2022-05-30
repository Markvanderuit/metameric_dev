#pragma once

#include <metameric/gui/detail/linear_scheduler/task.hpp>

namespace met {
  class ViewportBaseTask : public detail::AbstractTask {
  public:
    ViewportBaseTask(const std::string &name);
    void init(detail::TaskInitInfo &info) override;
    void eval(detail::TaskEvalInfo &info) override;
  };
} // namespace met