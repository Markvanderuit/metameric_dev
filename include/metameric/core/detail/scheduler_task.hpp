#pragma once

#include <metameric/core/math.hpp>
#include <metameric/core/utility.hpp>
#include <metameric/core/detail/trace.hpp>
#include <metameric/core/detail/scheduler_base.hpp>
#include <metameric/core/detail/scheduler_info.hpp>
#include <memory>
#include <list>
#include <string>
#include <unordered_map>

namespace met::detail {
  // Signal flags passed back by TaskInfo object
  // enum class TaskSignalFlags : uint {
    // eNone       = 0x000u,

    // Signal that tasks and owned resources are to be destroyed after run
    // eClearTasks = 0x001u,

    // Signal that tasks and all resources are to be destroyed after run
    // eClearAll   = 0x002u,
  // };
  // met_declare_bitflag(TaskSignalFlags);

  /**
   * Class that consumes application tasks and updates the environment
   * in which they exist.
   */
  // class LinearSchedulerHandle : public SchedulerHandle {
  //   // Private members
  //   LinearScheduler &m_scheduler;
  //   std::string      m_task_key;
  //   TaskSignalFlags  m_signal_flags;

  //   // Virtual method implementations
  //   virtual void      add_task_impl(AddTaskInfo &&info) override;
  //   virtual void      rem_task_impl(RemTaskInfo &&info) override;
  //   virtual RsrcBase *add_rsrc_impl(AddRsrcInfo &&info) override;
  //   virtual RsrcBase *get_rsrc_impl(GetRsrcInfo &&info) override;
  //   virtual void      rem_rsrc_impl(RemRsrcInfo &&info) override;

  //   virtual const std::string &task_key_impl() const override { 
  //     return m_task_key;
  //   };

  //   virtual void signal_clear_tasks_impl() override {
  //     m_signal_flags = TaskSignalFlags::eClearTasks;
  //   }

  //   virtual void signal_clear_all_impl() override {
  //     m_signal_flags = TaskSignalFlags::eClearAll;
  //   }

  // public:
  //   // Potential operations for scheduler task
  //   enum class Operation { eInit, eEval, eDstr };

  //   // Public members
  //   std::list<AddTaskInfo> add_task_info;
  //   std::list<RemTaskInfo> rem_task_info;
  //   std::list<AddRsrcInfo> add_rsrc_info;
  //   std::list<RemRsrcInfo> rem_rsrc_info;

  //   LinearSchedulerHandle(LinearScheduler &scheduler, const std::string &task_key)
  //   : m_scheduler(scheduler),
  //     m_task_key(task_key),
  //     m_signal_flags(TaskSignalFlags::eNone) { }

  //   const TaskSignalFlags &signal_flags() const { return m_signal_flags; }
  // };
} // met::detail