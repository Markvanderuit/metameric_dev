#pragma once

#include <metameric/core/detail/scheduler_base.hpp>
#include <metameric/core/math.hpp>
#include <memory>
#include <span>
#include <sstream>
#include <string>

namespace met {
  // Global key for resources with no managing task
  const std::string global_key = "global";
  
  // class TreeScheduler : public detail::TaskSchedulerBase,
  //                       public detail::RsrcSchedulerBase {
  //   // Internal resource/task node pair
  //   struct TreeNode { detail::RsrcNode rsrc; detail::TaskNode task; };

  //   // Private members
  //   std::unordered_map<std::string, TreeNode> m_tree_registry;
  //   std::vector<std::string>                  m_tree_order;
  
  //   // Virtual method implementations
  //   virtual const std::string &task_key_impl() const override { return global_key; }
  //   virtual void              add_task_impl(detail::AddTaskInfo &&) override;
  //   virtual void              rem_task_impl(detail::RemTaskInfo &&) override;
  //   virtual detail::RsrcNode &add_rsrc_impl(detail::AddRsrcInfo &&) override;
  //   virtual detail::RsrcNode &get_rsrc_impl(detail::GetRsrcInfo &&) override;
  //   virtual void              rem_rsrc_impl(detail::RemRsrcInfo &&) override;
    
  // public: /* public methods */
  //   // Scheduling
  //   void build();
  //   void run();

  //   // Miscellaneous
  //   void clear_tasks();  // Clear tasks and owned resources; retain global resources
  //   void clear_global(); // Clear global resources
  //   void clear_all();    // Clear tasks and resources
  // };

  class LinearScheduler : public detail::SchedulerBase {
    friend class LinearSchedulerHandle;

    // Private members
    detail::RsrcMap          m_rsrc_registry;
    detail::TaskMap          m_task_registry;
    std::vector<std::string> m_task_order;

    // Virtual method implementations
    virtual const std::string &task_key_impl() const override { return global_key; }
    virtual void              add_task_impl(AddTaskInfo &&) override;
    virtual void              rem_task_impl(RemTaskInfo &&) override;
    virtual detail::RsrcBase *add_rsrc_impl(AddRsrcInfo &&) override;
    virtual detail::RsrcBase *get_rsrc_impl(GetRsrcInfo &&) override;
    virtual void              rem_rsrc_impl(RemRsrcInfo &&) override;

  public: /* public methods */
    // Scheduling
    void build();
    void run();

    // Miscellaneous
    void clear_tasks();  // Clear tasks and owned resources; retain global resources
    void clear_global(); // Clear global resources
    void clear_all();    // Clear tasks and resources 

    // Debug
    virtual std::vector<std::string> schedule() const override {
      met_trace();
      return m_task_order;
    }

    // Debug
    virtual const detail::RsrcMap &resources() const override {
      met_trace();
      return m_rsrc_registry;
    }
  };

  // Signal flags passed back by LinearSchedulerHandle object
  enum class TaskSignalFlags : uint {
    eNone       = 0x000u,

    // Signal that tasks and owned resources are to be destroyed after run
    eClearTasks = 0x001u,

    // Signal that tasks and all resources are to be destroyed after run
    eClearAll   = 0x002u,
  };
  met_declare_bitflag(TaskSignalFlags);

  /**
   * Class that consumes application tasks and updates the environment
   * in which they exist.
   */
  class LinearSchedulerHandle : public detail::SchedulerHandle {
    // Private members
    LinearScheduler &m_scheduler;
    std::string      m_task_key;
    TaskSignalFlags  m_signal_flags;

    // Virtual method implementations
    virtual void              add_task_impl(AddTaskInfo &&info) override;
    virtual void              rem_task_impl(RemTaskInfo &&info) override;
    virtual detail::RsrcBase *add_rsrc_impl(AddRsrcInfo &&info) override;
    virtual detail::RsrcBase *get_rsrc_impl(GetRsrcInfo &&info) override;
    virtual void              rem_rsrc_impl(RemRsrcInfo &&info) override;

    virtual const std::string &task_key_impl() const override { 
      return m_task_key;
    };

    virtual void signal_clear_tasks_impl() override {
      m_signal_flags = TaskSignalFlags::eClearTasks;
    }

    virtual void signal_clear_all_impl() override {
      m_signal_flags = TaskSignalFlags::eClearAll;
    }

  public:
    // Public members
    std::list<AddTaskInfo> add_task_info;
    std::list<RemTaskInfo> rem_task_info;
    std::list<AddRsrcInfo> add_rsrc_info;
    std::list<RemRsrcInfo> rem_rsrc_info;

    LinearSchedulerHandle(LinearScheduler &scheduler, const std::string &task_key)
    : m_scheduler(scheduler),
      m_task_key(task_key),
      m_signal_flags(TaskSignalFlags::eNone) { }

    const TaskSignalFlags &signal_flags() const { return m_signal_flags; }

    // Debug
    virtual std::vector<std::string> schedule() const override {
      return m_scheduler.m_task_order;
    }
    
    // Debug
    virtual const detail::RsrcMap &resources() const override {
      met_trace();
      return m_scheduler.resources();
    }
  };
} // namespace met