#pragma once

#include <metameric/core/math.hpp>
#include <metameric/core/detail/scheduler_base.hpp>
#include <metameric/core/detail/scheduler_handle.hpp>
#include <unordered_map>

namespace met {
  struct MapBasedSchedule {
    using RsrcMap  = std::unordered_map<std::string, std::unordered_map<std::string, detail::RsrcBasePtr>>;
    using TaskMap  = std::unordered_map<std::string, detail::TaskBasePtr>;

    virtual const TaskMap &tasks()              const = 0;
    virtual const RsrcMap &resources()          const = 0;
    virtual std::vector<std::string> schedule() const = 0;
  };

  class LinearScheduler : public Scheduler,
                          public MapBasedSchedule {
  private:
    // Private members
    MapBasedSchedule::RsrcMap m_rsrc_registry;
    MapBasedSchedule::TaskMap m_task_registry;
    std::list<std::string>    m_schedule;

    // Virtual method implementations for Scheduler
    detail::TaskNode *add_task_impl(detail::TaskInfo &&)       override; // nullable return value
    detail::TaskNode *get_task_impl(detail::TaskInfo &&) const override; // nullable return value
    void              rem_task_impl(detail::TaskInfo &&)       override;
    detail::RsrcNode *add_rsrc_impl(detail::RsrcInfo &&)       override; // nullable return value
    detail::RsrcNode *get_rsrc_impl(detail::RsrcInfo &&) const override; // nullable return value
    void              rem_rsrc_impl(detail::RsrcInfo &&)       override;

  public: /* public methods */
    // Implementing handle class has access to internal components
    friend class LinearSchedulerHandle;
    
    // Virtual method implementations for Scheduler
    void run()                              override; // Run a single step of the schedule
    void clear(bool preserve_global = true) override; // Clear current schedule and resources

    // Virtual method implementations for MapBasedSchedule
    const TaskMap &tasks()              const override { return m_task_registry; }
    const RsrcMap &resources()          const override { return m_rsrc_registry; }
    std::vector<std::string> schedule() const override { return std::vector<std::string>(range_iter(m_schedule)); }
  };

  /**
   * Class that consumes application tasks and updates the environment
   * in which they exist.
   */
  class LinearSchedulerHandle : public SchedulerHandle,
                                public MapBasedSchedule {
  public:
    // Signal flags passed back by handle object back to scheduler
    enum class HandleReturnFlags : uint {
      eNone       = 0x000u, // Default value
      eClearTasks = 0x001u, // Signal that tasks and owned resources are to be destroyed after run
      eClearAll   = 0x002u, // Signal that tasks and global resources are to be destroyed after run
    };

  private:
    // Private members
    LinearScheduler &m_scheduler;

    // Friend private members
    HandleReturnFlags           return_flags;
    std::list<detail::TaskInfo> add_task_info;
    std::list<detail::TaskInfo> rem_task_info;

    // Virtual method implementations for SchedulerHandle
    detail::TaskNode *add_task_impl(detail::TaskInfo &&)       override; // nullable return value
    detail::TaskNode *get_task_impl(detail::TaskInfo &&) const override; // nullable return value
    void              rem_task_impl(detail::TaskInfo &&)       override;
    detail::RsrcNode *add_rsrc_impl(detail::RsrcInfo &&)       override; // nullable return value
    detail::RsrcNode *get_rsrc_impl(detail::RsrcInfo &&) const override; // nullable return value
    void              rem_rsrc_impl(detail::RsrcInfo &&)       override;

  public:
    // Implementing scheduler class has access to internal components
    friend class LinearScheduler;

    LinearSchedulerHandle(LinearScheduler &scheduler, const std::string &task_key)
    : SchedulerHandle(task_key),
      m_scheduler(scheduler),
      return_flags(HandleReturnFlags::eNone) { }

    // Virtual method implementations for SchedulerHandle
    void clear(bool preserve_global = true) override; // Clear current schedule and resources

    // Virtual method implementations for MapBasedSchedule
    const TaskMap &tasks()              const override { return m_scheduler.tasks(); }
    const RsrcMap &resources()          const override { return m_scheduler.resources(); }
    std::vector<std::string> schedule() const override { return m_scheduler.schedule(); }
  };

  // LinearSchedulerHandle::HandleReturnFlags function sugar
  met_declare_bitflag(LinearSchedulerHandle::HandleReturnFlags);
} // namespace met