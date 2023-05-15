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

  TaskHandle SchedulerHandle::task() {
    met_trace();
    return TaskHandle(this, { .task_key = m_task_key });
  }

  TaskHandle SchedulerHandle::subtask(const std::string &task_key) {
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
} // namespace met