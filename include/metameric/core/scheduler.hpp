// Copyright (C) 2024 Mark van de Ruit, Delft University of Technology.
// 
// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.
// 
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
// 
// You should have received a copy of the GNU General Public License
// along with this program.  If not, see <https://www.gnu.org/licenses/>.

#pragma once

#include <metameric/core/fwd.hpp>
#include <metameric/core/detail/scheduler_base.hpp>
#include <metameric/core/detail/scheduler_handle.hpp>
#include <list>
#include <unordered_map>

namespace met {
  struct MapBasedSchedule {
    using RsrcMap  = std::unordered_map<std::string, std::unordered_map<std::string, detail::RsrcBasePtr>>;
    using TaskMap  = std::unordered_map<std::string, detail::TaskBasePtr>;

    virtual const TaskMap &tasks()              const = 0;
    virtual const RsrcMap &resources()          const = 0;
    virtual std::vector<std::string> schedule() const = 0;
  };
  
  // Signal flags passed back by LinearSchedulerHandle object back to scheduler
  enum class LinearSchedulerHandleFlags : uint {
    eNone       = 0x000u, // Default value
    eClearTasks = 0x001u, // Signal that tasks and owned resources are to be destroyed after run
    eClearAll   = 0x002u, // Signal that tasks and global resources are to be destroyed after run
  };
  met_declare_bitflag(LinearSchedulerHandleFlags);
  
  // Implementing scheduler
  class LinearScheduler : public Scheduler,
                          public MapBasedSchedule {
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

  // Implementing scheduler handle, passed to task nodes during scheduler run
  class LinearSchedulerHandle : public SchedulerHandle,
                                public MapBasedSchedule {
    // Private members
    LinearScheduler &m_scheduler;

    // Friend private members
    LinearSchedulerHandleFlags  return_flags;
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
      return_flags(LinearSchedulerHandleFlags::eNone) { }

    // Virtual method implementations for SchedulerHandle
    void clear(bool preserve_global = true) override; // Clear current schedule and resources

    // Virtual method implementations for MapBasedSchedule
    const TaskMap &tasks()              const override { return m_scheduler.tasks(); }
    const RsrcMap &resources()          const override { return m_scheduler.resources(); }
    std::vector<std::string> schedule() const override { return m_scheduler.schedule(); }
  };
} // namespace met