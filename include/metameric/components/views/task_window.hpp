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

    bool handle_open(detail::SchedulerHandle &info);
    bool handle_save(detail::SchedulerHandle &info);
    bool handle_save_as(detail::SchedulerHandle &info);

    bool handle_export(detail::SchedulerHandle &info);

    void handle_close_safe(detail::SchedulerHandle &info);
    void handle_close(detail::SchedulerHandle &info);
    
    void handle_exit_safe(detail::SchedulerHandle &info);
    void handle_exit(detail::SchedulerHandle &info);

  public:
    void eval(detail::SchedulerHandle &info) override;
  };
} // namespace met