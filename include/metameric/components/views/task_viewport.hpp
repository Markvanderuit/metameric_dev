#pragma once

#include <metameric/core/scheduler.hpp>

namespace met {
  class ViewportTask : public detail::TaskBase {
  public:
    void init(detail::TaskInfo &) override;
    void dstr(detail::TaskInfo &) override;
    void eval(detail::TaskInfo &) override;
  };
} // namespace met