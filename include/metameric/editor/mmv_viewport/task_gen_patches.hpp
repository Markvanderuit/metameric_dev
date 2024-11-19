#pragma once

#include <metameric/editor/scene_viewport/task_input_editor.hpp>
#include <metameric/core/scheduler.hpp>

namespace met {
  class GenPatchesTask : public detail::TaskNode {

  public:
    bool is_active(SchedulerHandle &info) override;
    void init(SchedulerHandle &info) override;
    void eval(SchedulerHandle &info) override;
  };
} // namespace met