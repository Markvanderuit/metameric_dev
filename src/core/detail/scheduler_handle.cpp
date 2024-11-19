#include <metameric/core/detail/scheduler_handle.hpp>

namespace met {
  ResourceHandle::ResourceHandle(detail::SchedulerBase *schd_handle, detail::RsrcInfo key)
  : m_rsrc_key(key), 
    m_schd_handle(schd_handle), 
    m_rsrc_handle(m_schd_handle->get_rsrc_impl(std::move(key))) { }

  TaskHandle::TaskHandle(detail::SchedulerBase *schd_handle, detail::TaskInfo key)
  : m_task_key(key), 
    m_schd_handle(schd_handle), 
    m_task_handle(m_schd_handle->get_task_impl(std::move(key))) { }

  MaskedSchedulerHandle TaskHandle::mask(SchedulerHandle &handle) const {
    return MaskedSchedulerHandle(handle, key());
  }
} // namespace met