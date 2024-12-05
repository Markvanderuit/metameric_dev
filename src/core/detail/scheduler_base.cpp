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

#include <metameric/core/detail/scheduler_base.hpp>
#include <metameric/core/detail/scheduler_handle.hpp>
#include <algorithm>

namespace met {
  // String key referring to global, non-owned resources in scheduler resource management
  static const std::string global_key = "global";

  TaskHandle detail::SchedulerBase::task(const std::string &task_key) {
    met_trace();
    return TaskHandle(this, { .task_key = task_key });
  }

  ResourceHandle detail::SchedulerBase::global(const std::string &rsrc_key) {
    met_trace();
    return ResourceHandle(this, { .task_key = global_key, .rsrc_key = rsrc_key });
  }

  ResourceHandle detail::SchedulerBase::resource(const std::string &task_key, const std::string &rsrc_key) {
    met_trace();
    return ResourceHandle(this, { .task_key = task_key, .rsrc_key = rsrc_key });
  }

  ResourceHandle detail::SchedulerBase::operator()(const std::string &task_key, const std::string &rsrc_key) {
    met_trace();
    return ResourceHandle(this, { .task_key = task_key, .rsrc_key = rsrc_key });
  }

  ResourceHandle SchedulerHandle::resource(const std::string &rsrc_key) {
    met_trace();
    return ResourceHandle(this, { .task_key = m_task_key, .rsrc_key = rsrc_key });
  }
  
  ResourceHandle SchedulerHandle::operator()(const std::string &rsrc_key) {
    met_trace();
    return ResourceHandle(this, { .task_key = m_task_key, .rsrc_key = rsrc_key });
  }

  std::vector<ResourceHandle> resources() {
    met_trace();
    std::vector<ResourceHandle> handles;
    // ...
    return handles;
  }

  TaskHandle SchedulerHandle::task() {
    met_trace();
    return TaskHandle(this, { .task_key = m_task_key });
  }

  TaskHandle SchedulerHandle::child_task(const std::string &task_key) {
    met_trace();
    return TaskHandle(this, { .prnt_key = m_task_key, .task_key = task_key });
  }

  TaskHandle SchedulerHandle::parent_task() {
    met_trace();
    if (auto pos = m_task_key.find_last_of('.'); pos != std::string::npos) {
      std::string task_key(m_task_key.begin(), m_task_key.begin() + pos);
      return TaskHandle(this, { .task_key = task_key });
    } else {
      return TaskHandle(this, { });
    }
  }

  TaskHandle SchedulerHandle::relative_task(const std::string &task_key) {
    met_trace();
    if (auto pos = m_task_key.find_last_of('.'); pos != std::string::npos) {
      std::string prnt_key(m_task_key.begin(), m_task_key.begin() + pos);
      return TaskHandle(this, { .prnt_key = prnt_key, .task_key = task_key });
    } else {
      return TaskHandle(this, { .task_key = task_key });
    }
  }

  MaskedSchedulerHandle SchedulerHandle::parent() {
    met_trace();
    return parent_task().mask(*this);
  }

  MaskedSchedulerHandle SchedulerHandle::child(const std::string &task_key) {
    met_trace();
    return child_task(task_key).mask(*this);
  }

  MaskedSchedulerHandle SchedulerHandle::relative(const std::string &task_key) {
    met_trace();
    return relative_task(task_key).mask(*this);
  }
} // namespace met