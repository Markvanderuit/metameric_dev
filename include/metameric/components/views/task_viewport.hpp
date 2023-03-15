#pragma once

#include <metameric/core/scheduler.hpp>

namespace met {
  class ViewportTask : public detail::AbstractTask {
  public:
    ViewportTask(const std::string &name);

    void init(detail::TaskInfo &) override;
    void dstr(detail::TaskInfo &) override;
    void eval(detail::TaskInfo &) override;
  };
} // namespace met