#pragma once

#include <metameric/core/scheduler.hpp>

namespace met {
  class SceneComponentsEditorTask : public detail::TaskNode {
    void eval_objects(SchedulerHandle &info);
    void eval_emitters(SchedulerHandle &info);
    void eval_upliftings(SchedulerHandle &info);
    void eval_colr_systems(SchedulerHandle &info);

  public:
    void eval(SchedulerHandle &info) override;
  };
} // namespace met