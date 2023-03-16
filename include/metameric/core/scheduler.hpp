#pragma once

#include <metameric/core/detail/scheduler_task.hpp>
#include <memory>
#include <span>
#include <sstream>
#include <string>

namespace met {
  // Global key for resources with no managing task
  const std::string global_key = "global";
  
  class LinearScheduler : public detail::TaskSchedulerBase,
                          public detail::RsrcSchedulerBase {
  private: /* private members */
    detail::RsrcMap          m_rsrc_registry;
    detail::TaskMap          m_task_registry;
    std::vector<std::string> m_task_order;

  private: /* virtual methods */
    virtual const std::string &task_key_impl() const override { return global_key; }
    virtual void             add_task_impl(detail::AddTaskInfo &&) override;
    virtual void             rem_task_impl(detail::RemTaskInfo &&) override;
    virtual detail::RsrcNode add_rsrc_impl(detail::AddRsrcInfo &&) override;
    virtual detail::RsrcNode get_rsrc_impl(detail::GetRsrcInfo &&) override;
    virtual void             rem_rsrc_impl(detail::RemRsrcInfo &&) override;

  public: /* public methods */
    // Scheduling
    void build();
    void run();

    // Miscellaneous
    void clear_tasks();  // Clear tasks and owned resources; retain global resources
    void clear_global(); // Clear global resources
    void clear_all();    // Clear tasks and resources 

    // Debug
    const auto& tasks()      const { return m_task_registry; }
    const auto& resources()  const { return m_rsrc_registry; }
    const auto& task_order() const { return m_task_order;    }
  };
} // namespace met