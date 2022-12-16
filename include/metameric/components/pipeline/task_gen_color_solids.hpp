#pragma once

#include <metameric/core/mesh.hpp>
#include <metameric/core/scheduler.hpp>

namespace met {
  class GenColorSolidsTask : public detail::AbstractTask {
  public:
    GenColorSolidsTask(const std::string &name);
    void init(detail::TaskInitInfo &) override;
    void eval(detail::TaskEvalInfo &) override;
  };
} // namespace met