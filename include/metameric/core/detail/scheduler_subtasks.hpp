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