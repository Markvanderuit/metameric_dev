#pragma once

#include <metameric/core/mesh.hpp>
#include <metameric/core/scheduler.hpp>

namespace met {
  class GenColorSolidsTask : public detail::AbstractTask {
  public:
    GenColorSolidsTask(const std::string &name);
    void init(detail::TaskInfo &) override;
    void eval(detail::TaskInfo &) override;
  };
} // namespace met