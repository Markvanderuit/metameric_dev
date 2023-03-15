#pragma once

#include <metameric/core/scheduler.hpp>
#include <string>

namespace met {
  class WindowTask : public detail::AbstractTask {
    /* Local state for handling modals */
    bool m_open_close_modal; 
    bool m_open_exit_modal; 
    bool m_open_create_modal; 

    /* Special handlers */

    bool handle_open(detail::TaskInfo &info);
    bool handle_save(detail::TaskInfo &info);
    bool handle_save_as(detail::TaskInfo &info);

    bool handle_export(detail::TaskInfo &info);

    void handle_close_safe(detail::TaskInfo &info);
    void handle_close(detail::TaskInfo &info);
    
    void handle_exit_safe(detail::TaskInfo &info);
    void handle_exit(detail::TaskInfo &info);

  public:
    WindowTask(const std::string &name);
    
    void init(detail::TaskInfo &info) override;
    void dstr(detail::TaskInfo &info) override;
    void eval(detail::TaskInfo &info) override;
  };
} // namespace met