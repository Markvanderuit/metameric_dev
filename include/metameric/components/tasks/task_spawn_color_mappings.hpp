#pragma once

#include <metameric/core/math.hpp>
#include <metameric/core/scheduler.hpp>

namespace met {
  class SpawnColorMappingsTask : public detail::AbstractTask {
    uint m_tasks_n;

  public:
    SpawnColorMappingsTask(const std::string &name);

    void init(detail::TaskInitInfo &info) override;
    void dstr(detail::TaskDstrInfo &info) override;
    void eval(detail::TaskEvalInfo &info) override;
  };
} // namespace met