#include <metameric/core/detail/scheduler_base.hpp>
#include <metameric/core/detail/scheduler_handle.hpp>

namespace met {
  // String key referring to global, non-owned resources in scheduler resource management
  static const std::string global_key = "global";

  TaskHandle detail::SchedulerBase::task(const std::string &task_key) {
    met_trace();
    return TaskHandle(this, { .task_key = task_key });
  }

  ResourceHandle SchedulerHandle::global(const std::string &rsrc_key) {
    met_trace();
    return ResourceHandle(this, { .task_key = global_key, .rsrc_key = rsrc_key });
  }
  
  ResourceHandle SchedulerHandle::resource(const std::string &rsrc_key) {
    met_trace();
    return ResourceHandle(this, { .task_key = m_task_key, .rsrc_key = rsrc_key });
  }

  ResourceHandle SchedulerHandle::resource(const std::string &task_key, const std::string &rsrc_key) {
    met_trace();
    return ResourceHandle(this, { .task_key = task_key, .rsrc_key = rsrc_key });
  }

  TaskHandle SchedulerHandle::task() {
    met_trace();
    return TaskHandle(this, { .task_key = m_task_key });
  }

  TaskHandle SchedulerHandle::subtask(const std::string &task_key) {
    met_trace();
    return TaskHandle(this, { .prnt_key = m_task_key, .task_key = task_key });
  }

  ResourceHandle Scheduler::global(const std::string &rsrc_key) {
    met_trace();
    return ResourceHandle(this, { .task_key = global_key, .rsrc_key = rsrc_key });
  }

  ResourceHandle Scheduler::resource(const std::string &task_key, const std::string &rsrc_key) {
    met_trace();
    return ResourceHandle(this, { .task_key = task_key, .rsrc_key = rsrc_key });
  }
} // namespace met