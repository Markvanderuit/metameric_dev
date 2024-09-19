#pragma once

#include <metameric/core/scheduler.hpp>

namespace met {
  class WindowTask : public detail::TaskNode {
    bool m_open_close_modal; 
    bool m_open_exit_modal;

    void handle_close_safe(SchedulerHandle &info);
    void handle_exit_safe(SchedulerHandle &info);
    
  public:
    void eval(SchedulerHandle &info) override;
  };
} // namespace met