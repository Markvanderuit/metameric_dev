#pragma once

#include <metameric/core/scheduler.hpp>
#include <string>

namespace met {
  class WindowTask : public detail::AbstractTask {
    /* Local state for handling modals */
    bool m_open_save_close_modal; 
    bool m_open_save_exit_modal; 
    bool m_open_new_project_modal; 

    /* Special handlers */

    void handle_open(detail::TaskEvalInfo &info);
    void handle_save(detail::TaskEvalInfo &info);
    void handle_save_as(detail::TaskEvalInfo &info);

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