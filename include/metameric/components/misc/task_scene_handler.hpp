#pragma once

#include <metameric/core/scene_handler.hpp>
#include <metameric/core/scheduler.hpp>

namespace met {
  struct SceneHandlerTask : public detail::TaskNode {
    bool is_active(SchedulerHandle &info) override {
      const auto &e_handler = info.global("scene_handler").read_only<SceneHandler>();
    }
    
    void eval(SchedulerHandle &info) override {
      met_trace_full();
      
    }
  };
} // namespace met