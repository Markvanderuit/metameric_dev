#pragma once

#include <metameric/core/math.hpp>
#include <metameric/core/detail/trace.hpp>
#include <metameric/core/detail/scheduler_base.hpp>
#include <functional>

namespace met::detail {
  template <typename TaskType>
  class Subtasks {
    using KeyType = std::string;
    using AddType = std::function<std::pair<std::string, TaskType> (detail::SchedulerHandle &, uint)>;
    using RmvType = std::function<std::string                      (detail::SchedulerHandle &, uint)>;
    
    uint    m_n_tasks = 0;
    AddType m_add;
    RmvType m_rmv;

    void adjust_to(detail::SchedulerHandle &info, uint n_tasks) {
      met_trace_full();

      // Adjust nr. of subtasks upwards if necessary
      for (; m_n_tasks < n_tasks; ++m_n_tasks) {
        auto [key, task] = m_add(info, m_n_tasks);
        info.insert_subtask(info.task_key(), key, std::move(task));
      }

      // Adjust nr. of subtasks downwards if necessary
      for (; m_n_tasks > n_tasks; --m_n_tasks) {
        auto key = m_rmv(info, m_n_tasks - 1);
        info.remove_subtask(info.task_key(), key);
      }
    }

  public:
    void init(detail::SchedulerHandle &info, uint n_tasks, AddType add, RmvType rmv) {
      met_trace_full();

      // Clear out remaining tasks
      adjust_to(info, 0);
      
      m_add  = add;
      m_rmv  = rmv;
 
      // Spawn initial subtasks
      adjust_to(info, n_tasks);
    }

    void eval(detail::SchedulerHandle &info, uint n_tasks) {
      met_trace_full();
      adjust_to(info, n_tasks);
    }

    void dstr(detail::SchedulerHandle &info) {
      met_trace_full();
      adjust_to(info, 0);
    }
  };
} // met::detail