#include <metameric/core/detail/scheduler_task.hpp>

namespace met::detail {
  /* void LinearSchedulerHandle::add_task_impl(AddTaskInfo &&info) {
    met_trace();
    add_task_info.emplace_back(std::move(info));
  }

  void LinearSchedulerHandle::rem_task_impl(RemTaskInfo &&info) {
    met_trace();
    rem_task_info.emplace_back(std::move(info));
  }

  RsrcBase *LinearSchedulerHandle::add_rsrc_impl(AddRsrcInfo &&info) {
    met_trace();
    return add_rsrc_info.emplace_back(std::move(info)).rsrc.get();
  }

  RsrcBase *LinearSchedulerHandle::get_rsrc_impl(GetRsrcInfo &&info) {
    met_trace();

    // Find relevant task resources key/value iterator
    auto task_it = m_scheduler.m_rsrc_registry.find(info.task_key);
    guard(task_it != m_scheduler.m_rsrc_registry.end(), nullptr);
    
    // Find relevant resource key/value iterator
    auto rsrc_it = task_it->second.find(info.rsrc_key);
    guard(rsrc_it != task_it->second.end(), nullptr);

    return rsrc_it->second.get();
  }

  void LinearSchedulerHandle::rem_rsrc_impl(RemRsrcInfo &&info) {
    met_trace();
    rem_rsrc_info.emplace_back(std::move(info));
  } */
} // namespace met::detail