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
  class TaskInfoRsrcScheduler : public RsrcSchedulerBase {
    // Virtual methods from RsrcSchedulerBase
    virtual const std::string &task_key_impl() const override { return m_task_key_impl; };
    virtual RsrcNode add_rsrc_impl(AddRsrcInfo &&info) override;
    virtual RsrcNode get_rsrc_impl(GetRsrcInfo &&info) override;
    virtual void     rem_rsrc_impl(RemRsrcInfo &&info) override;

  protected:
    std::string  m_task_key_impl;
    RsrcMap     &m_rsrc_registry;

  public:
    // Public data registries
    std::list<AddRsrcInfo> add_rsrc_info;
    std::list<RemRsrcInfo> rem_rsrc_info;
    
    /* Public constructor */
    
    TaskInfoRsrcScheduler(RsrcMap &rsrc_registry, const std::string &task_key)
    : m_rsrc_registry(rsrc_registry),
      m_task_key_impl(task_key) { }

    /* Miscellaneous, debug info */
    
    const RsrcMap& resources() const { return m_rsrc_registry; }
  };

  class TaskInfoTaskScheduler : public TaskSchedulerBase {
    virtual void add_task_impl(AddTaskInfo &&info) override;
    virtual void rem_task_impl(RemTaskInfo &&info) override;

  protected:
    TaskMap &m_appl_task_registry;

  public:
    std::list<AddTaskInfo> add_task_info;
    std::list<RemTaskInfo> rem_task_info;
    
    TaskInfoTaskScheduler(TaskMap &appl_task_registry)
    : m_appl_task_registry(appl_task_registry) {}

    const TaskMap& tasks() const { return m_appl_task_registry; }
  };

  // Usage flags passed into TaskInfo object
  enum class TaskInfoUsage { eInit, eEval, eDstr };

  // Signal flags passed back by TaskInfo object
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
  class TaskInfo : public TaskInfoRsrcScheduler, 
                   public TaskInfoTaskScheduler {
    std::string m_task_key;

  public:
    TaskInfo(RsrcMap           &appl_rsrc_registry,
             TaskMap           &appl_task_registry,
             const std::string &key,
             TaskNode          &task,
             TaskInfoUsage      usage)
    : TaskInfoRsrcScheduler(appl_rsrc_registry, key),
      TaskInfoTaskScheduler(appl_task_registry),
      m_task_key(key) {
      met_trace();
      switch (usage) {
        case TaskInfoUsage::eInit: task->init(*this); break;
        case TaskInfoUsage::eEval: task->eval(*this); break;
        case TaskInfoUsage::eDstr: task->dstr(*this); break;
      }
    }

    TaskSignalFlags signal_flags = TaskSignalFlags::eNone;

    const std::string &task_key() const { return m_task_key; }
  };
} // met::detail