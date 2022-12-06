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

    bool handle_open(detail::TaskEvalInfo &info);
    bool handle_save(detail::TaskEvalInfo &info);
    bool handle_save_as(detail::TaskEvalInfo &info);

    bool handle_export(detail::TaskEvalInfo &info);

    void handle_close_safe(detail::TaskEvalInfo &info);
    void handle_close(detail::TaskEvalInfo &info);
    
    void handle_exit_safe(detail::TaskEvalInfo &info);
    void handle_exit(detail::TaskEvalInfo &info);

  public:
    WindowTask(const std::string &name);
    
    void init(detail::TaskInitInfo &info) override;
    void dstr(detail::TaskDstrInfo &info) override;
    void eval(detail::TaskEvalInfo &info) override;
  };
} // namespace met