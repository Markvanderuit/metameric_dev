#pragma once

#include <metameric/core/scene.hpp>
#include <metameric/core/scheduler.hpp>
#include <metameric/core/detail/scheduler_subtasks.hpp>

namespace met {
  class GenObjectDataTask : public detail::TaskNode {
    uint m_object_i;

  public:
    GenObjectDataTask(uint object_i);

    bool is_active(SchedulerHandle &) override;
    void init(SchedulerHandle &)      override;
    void eval(SchedulerHandle &)      override;
  };

  class GenObjectsTask : public detail::TaskNode {
    detail::Subtasks<GenObjectDataTask> m_subtasks;

  public:
    void init(SchedulerHandle &info) override {
      met_trace();

      // Get external resources
      const auto &e_scene   = info.global("scene").getr<Scene>();
      const auto &e_objects = e_scene.components.objects;

      // Add subtasks to perform mapping
      m_subtasks.init(info, e_objects.size(), 
        [](uint i)         { return fmt::format("gen_object_{}", i); },
        [](auto &, uint i) { return GenObjectDataTask(i);                });
    }

    void eval(SchedulerHandle &info) override {
      met_trace();

      // Get external resources
      const auto &e_scene   = info.global("scene").getr<Scene>();
      const auto &e_objects = e_scene.components.objects;

      // Adjust nr. of subtasks
      m_subtasks.eval(info, e_objects.size());
    }
  };
} // namespace met