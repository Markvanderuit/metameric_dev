#pragma once

#include <metameric/components/views/mesh_viewport/task_input_editor.hpp>
#include <metameric/core/scheduler.hpp>

namespace met {
  struct MMVEditorTask : public detail::TaskNode  {
    InputSelection m_is; // Active input selection

  public:
    // Constructor; the task is specified for a specific,
    // selected constraint for now
    MMVEditorTask(InputSelection is) : m_is(is) { }

    void init(SchedulerHandle &info) override;
    void eval(SchedulerHandle &info) override;
    bool is_active(SchedulerHandle &info) override;
  };
} // namespace met