#include <metameric/core/detail/scheduler_base.hpp>

namespace met {
  ResourceHandle::ResourceHandle(detail::SchedulerBase *schd_handle, RsrcInfo key)
  : m_rsrc_key(key), 
    m_schd_handle(schd_handle), 
    m_rsrc_handle(m_schd_handle->get_rsrc_impl(std::move(key))) { }

  TaskHandle::TaskHandle(detail::SchedulerBase *schd_handle, TaskInfo key)
  : m_task_key(key), 
    m_schd_handle(schd_handle), 
    m_task_handle(m_schd_handle->get_task_impl(std::move(key))) { }

  ResourceHandle detail::SchedulerBase::resource(const std::string &task_key, const std::string &rsrc_key) {
    met_trace();
    return ResourceHandle(this, { .task_key = task_key, .rsrc_key = rsrc_key });
  }

  ResourceHandle detail::SchedulerBase::resource(const std::string &rsrc_key) {
    met_trace();
    return ResourceHandle(this, { .task_key = task_default_key(), .rsrc_key = rsrc_key });
  }

  TaskHandle detail::SchedulerBase::task(const std::string &task_key) {
    met_trace();
    return TaskHandle(this, { .task_key = task_key });
  }

  TaskHandle SchedulerHandle::subtask(const std::string &task_key) {
    met_trace();
    return TaskHandle(this, { .prnt_key = task_default_key(), .task_key = task_key });
  }
}