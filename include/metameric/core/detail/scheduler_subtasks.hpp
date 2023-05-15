#pragma once

#include <metameric/core/math.hpp>
#include <metameric/core/detail/trace.hpp>
#include <metameric/core/detail/scheduler_base.hpp>
#include <functional>

namespace met::detail {
  template <typename TaskType>
  class Subtasks {
    using AddFuncType = std::function<TaskType    (SchedulerHandle &, uint)>;
    using KeyFuncType = std::function<std::string (uint)>;
    
    uint        m_n_tasks = 0;
    AddFuncType m_add_func;
    KeyFuncType m_key_func;

    void adjust_to(SchedulerHandle &info, uint n_tasks) {
      met_trace();

      // Adjust nr. of subtasks upwards if necessary
      for (; m_n_tasks < n_tasks; ++m_n_tasks)
        info.child_task(m_key_func(m_n_tasks)).set(m_add_func(info, m_n_tasks));

      // Adjust nr. of subtasks downwards if necessary
      for (; m_n_tasks > n_tasks; --m_n_tasks)
        info.child_task(m_key_func(m_n_tasks - 1)).dstr();
    }

  public:
    void init(SchedulerHandle &info, uint n_tasks, KeyFuncType key_func, AddFuncType add_func) {
      met_trace();

      // Clear out remaining tasks
      adjust_to(info, 0);
      
      m_add_func = add_func;
      m_key_func = key_func;
 
      // Spawn initial subtasks
      adjust_to(info, n_tasks);
    }

    void eval(SchedulerHandle &info, uint n_tasks) {
      met_trace();
      adjust_to(info, n_tasks);
    }

    void dstr(SchedulerHandle &info) {
      met_trace();
      adjust_to(info, 0);
    }
  };
} // met::detail