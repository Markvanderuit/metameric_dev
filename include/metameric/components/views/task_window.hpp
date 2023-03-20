#pragma once

#include <metameric/core/scheduler.hpp>
#include <string>

namespace met {
  class WindowTask : public detail::TaskBase {
    /* Local state for handling modals */
    bool m_open_close_modal; 
    bool m_open_exit_modal; 
    bool m_open_create_modal; 

    /* Special handlers */

    bool handle_open(SchedulerHandle &info);
    bool handle_save(SchedulerHandle &info);
    bool handle_save_as(SchedulerHandle &info);

    bool handle_export(SchedulerHandle &info);

    void handle_close_safe(SchedulerHandle &info);
    void handle_close(SchedulerHandle &info);
    
    void handle_exit_safe(SchedulerHandle &info);
    void handle_exit(SchedulerHandle &info);

  public:
    void eval(SchedulerHandle &info) override;
  };
} // namespace met