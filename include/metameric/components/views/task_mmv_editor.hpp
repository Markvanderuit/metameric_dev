#pragma once

#include <metameric/components/views/scene_viewport/task_input_editor.hpp>
#include <metameric/core/scheduler.hpp>

namespace met {
  struct MMVEditorTask : public detail::TaskNode  {
    ConstraintRecord m_cs; // Active input selection

    // Constructor; the task is specified for a specific,
    // selected constraint for now
    MMVEditorTask(ConstraintRecord is) : m_cs(is) { }

    void init(SchedulerHandle &info) override;
    void eval(SchedulerHandle &info) override;
  };
} // namespace met