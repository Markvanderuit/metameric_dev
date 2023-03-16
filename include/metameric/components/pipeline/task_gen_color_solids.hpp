#pragma once

#include <metameric/core/mesh.hpp>
#include <metameric/core/scheduler.hpp>

namespace met {
  class GenColorSolidsTask : public detail::TaskBase {
  public:
    void init(detail::TaskInfo &) override;
    void eval(detail::TaskInfo &) override;
  };
} // namespace met