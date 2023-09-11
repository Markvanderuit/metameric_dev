#pragma once

#include <metameric/core/scene_handler.hpp>
#include <metameric/core/scheduler.hpp>
#include <metameric/components/misc/detail/scene.hpp>

namespace met {
  class SceneHandlerTask : public detail::TaskNode {
    bool m_is_init = false;

  public:
    void init(SchedulerHandle &info) override;
    void eval(SchedulerHandle &info) override;
  };
} // namespace met