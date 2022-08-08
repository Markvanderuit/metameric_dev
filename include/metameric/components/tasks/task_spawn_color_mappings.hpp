#pragma once

#include <metameric/core/scheduler.hpp>

namespace met {
  class SpawnColorMappingsTask : public detail::AbstractTask {
  public:
    SpawnColorMappingsTask(const std::string &name);

    void init(detail::TaskInitInfo &info) override;
    void dstr(detail::TaskDstrInfo &info) override;
    void eval(detail::TaskEvalInfo &info) override;
  };
} // namespace met