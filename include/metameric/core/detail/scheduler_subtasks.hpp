#pragma once

#include <metameric/core/math.hpp>
#include <metameric/core/detail/trace.hpp>
#include <metameric/core/detail/scheduler_task.hpp>
#include <functional>

namespace met::detail {
  template <typename TaskType>
  class Subtasks {
    using KeyType = std::string;
    using InfType = detail::TaskInfo;
    using AddType = std::function<TaskType(InfType &, uint)>;
    using RmvType = std::function<KeyType (InfType &, uint)>;
    
    uint    m_n_tasks = 0;
    KeyType m_prev;
    AddType m_add;
    RmvType m_rmv;

    void adjust_to(InfType &info, uint n_tasks) {
      met_trace_full();

      // Adjust nr. of subtasks upwards if necessary
      for (; m_n_tasks < n_tasks; ++m_n_tasks) {
        auto task = m_add(info, m_n_tasks);
        info.insert_task_after(m_prev, std::move(task));
      }

      // Adjust nr. of subtasks downwards if necessary
      for (; m_n_tasks > n_tasks; --m_n_tasks) {
        info.remove_task(m_rmv(info, m_n_tasks - 1));
      }
    }

  public:
    void init(const KeyType &prev, InfType &info, uint n_tasks, AddType add, RmvType rmv) {
      met_trace_full();

      // Clear out remaining tasks
      adjust_to(info, 0);
      
      m_prev = prev;
      m_add  = add;
      m_rmv  = rmv;
 
      // Spawn initial subtasks
      adjust_to(info, n_tasks);
    }

    void eval(InfType &info, uint n_tasks) {
      met_trace_full();
      adjust_to(info, n_tasks);
    }

    void dstr(detail::TaskInfo &info) {
      met_trace_full();
      adjust_to(info, 0);
    }
  };
} // met::detail