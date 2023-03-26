#include <metameric/core/scheduler.hpp>
#include <metameric/core/detail/trace.hpp>
#include <algorithm>
#include <execution>
#include <list>
#include <ranges>
#include <fmt/core.h>

namespace met {
  detail::TaskNode *LinearScheduler::add_task_impl(detail::TaskInfo &&info) {
    met_trace();

    // Final task key is parent_key.child_key
    std::string task_key = info.prnt_key.empty() 
                         ? info.task_key
                         : fmt::format("{}.{}", info.prnt_key, info.task_key);

    // Parse task info object by consuming task
    LinearSchedulerHandle handle(*this, task_key);
    info.ptr->init(handle);

    // Move task into registry
    auto it = m_task_registry.emplace(std::pair { task_key, std::move(info.ptr) }).first;
    if (info.prnt_key.empty()) {
      // No previous task key provided; insert at end of list
      m_schedule.emplace_back(task_key);
    } else {
      // Parent task key provided; Find end of range of said parent's other subtasks
      auto it = std::ranges::find_if(std::views::reverse(m_schedule), 
        [&](const auto &s) { return s.starts_with(info.prnt_key); }).base();
      m_schedule.emplace(it, task_key);
    }
    
    // Update task registries
    for (auto &info : handle.rem_task_info) rem_task_impl(std::move(info));
    for (auto &info : handle.add_task_info) add_task_impl(std::move(info));

    return it->second.get();
  }

  detail::TaskNode *LinearScheduler::get_task_impl(detail::TaskInfo &&info) const {
    met_trace();
    
    // Final task key is parent_key.child_key
    std::string task_key = info.prnt_key.empty() 
                         ? info.task_key
                         : fmt::format("{}.{}", info.prnt_key, info.task_key);

    auto it = m_task_registry.find(task_key);
    guard(it != m_task_registry.end(), nullptr);

    return it->second.get();
  }

  void LinearScheduler::rem_task_impl(detail::TaskInfo &&info) {
    met_trace();

    // Final task key is parent_key.child_key
    std::string task_key = info.prnt_key.empty() 
                         ? info.task_key
                         : fmt::format("{}.{}", info.prnt_key, info.task_key);

    // Find range of tasks with task_key as key, or task_key as parent
    auto filt = m_schedule 
              | std::views::filter([&](const auto &s) { return s.starts_with(task_key); })
              | std::views::reverse;
    guard(!filt.empty());

    // Iterate removable subtasks in reverse order
    std::vector<std::string> remove_range(range_iter(filt));
    for (const auto &remove_key : remove_range) {
      auto it = std::ranges::find(m_schedule, remove_key);
      guard(it != m_schedule.end() && m_task_registry.contains(remove_key));

      // Create handle object and let task run
      LinearSchedulerHandle handle(*this, remove_key);
      m_task_registry.at(remove_key)->dstr(handle);
  
      // Update registries; remove task and resources
      m_task_registry.erase(remove_key);
      m_rsrc_registry.erase(remove_key);
      m_schedule.erase(it);

      // Update task registry; add/remove other specified subtasks
      for (auto &info : handle.rem_task_info) rem_task_impl(std::move(info));
      for (auto &info : handle.add_task_info) add_task_impl(std::move(info));
    }
  }

  detail::RsrcNode *LinearScheduler::add_rsrc_impl(detail::RsrcInfo &&info) {
    met_trace();

    // Insert s.t. resource registry is created if nonexistent
    auto [it, _] = m_rsrc_registry[info.task_key].insert_or_assign(info.rsrc_key, std::move(info.ptr));
    return it->second.get();
  }

  detail::RsrcNode *LinearScheduler::get_rsrc_impl(detail::RsrcInfo &&info) const {
    met_trace();
    
    // Find relevant task resources key/value iterator
    auto task_it = m_rsrc_registry.find(info.task_key);
    guard(task_it != m_rsrc_registry.end(), nullptr);
    
    // Find relevant resource key/value iterator
    auto rsrc_it = task_it->second.find(info.rsrc_key);
    guard(rsrc_it != task_it->second.end(), nullptr);

    return rsrc_it->second.get();
  }

  void LinearScheduler::rem_rsrc_impl(detail::RsrcInfo &&info) {
    met_trace();
    
    // Given existence of the specified task, erase specified resource
    if (auto it = m_rsrc_registry.find(info.task_key); it != m_rsrc_registry.end()) {
      fmt::print("rem_rsrc_impl {} - {}\n", info.task_key, info.rsrc_key);
      it->second.erase(info.rsrc_key);
    }
  }

  void LinearScheduler::run() {
    using Flags = LinearSchedulerHandle::HandleReturnFlags;

    met_trace();

    // Run all current tasks in copy of schedule order
    for (const auto &task_key : std::list<std::string>(m_schedule)) {
      // Find iterators to task node and set of rsrc nodes
      auto task_it = m_task_registry.find(task_key);
      auto rsrc_it = m_rsrc_registry.find(task_key);

      // If task node doesn't/no longer exists, skip
      guard_continue(task_it != m_task_registry.end());
      detail::TaskBasePtr task = task_it->second;

      // If resource nodes exist, reset mutate state
      if (rsrc_it != m_rsrc_registry.end())
        std::ranges::for_each(rsrc_it->second, [](auto &pair) { pair.second->set_mutated(false); });

      // Parse task info object by consuming task::eval_state() and task::eval()
      LinearSchedulerHandle handle(*this, task_key);
      guard_continue(task->is_active(handle));
      task->eval(handle);

      // Process signal flags; clear tasks/resources if requested
      if (has_flag(handle.return_flags, Flags::eClearTasks)) clear();
      if (has_flag(handle.return_flags, Flags::eClearAll))   clear(false);
      
      // Defer task updates until current task is complete
      for (auto &info : handle.rem_task_info) rem_task_impl(std::move(info));
      for (auto &info : handle.add_task_info) add_task_impl(std::move(info));

      // Signal flag received; halt current run
      guard_break(!static_cast<uint>(handle.return_flags)); 
    }
  }
  
  void LinearScheduler::clear(bool preserve_global) {
    met_trace();
    if (preserve_global) {
      std::list<std::string> task_order_copy(m_schedule);
      std::ranges::for_each(task_order_copy, 
        [&](const auto &key) { rem_task_impl({ .task_key = key }); });
    } else {
      m_rsrc_registry.clear();
      m_task_registry.clear();
      m_schedule.clear();
    }
  }

  void LinearSchedulerHandle::clear(bool preserve_global) {
    met_trace();
    return_flags |= (preserve_global ? HandleReturnFlags::eClearTasks : HandleReturnFlags::eClearAll);
  }

  detail::TaskNode *LinearSchedulerHandle::add_task_impl(detail::TaskInfo &&info) {
    met_trace();
    add_task_info.emplace_back(std::move(info));
    return nullptr; // Task add is deferred to end of run, so cannot return
  }

  void LinearSchedulerHandle::rem_task_impl(detail::TaskInfo &&info) {
    met_trace();
    rem_task_info.emplace_back(std::move(info));
  }

  detail::TaskNode *LinearSchedulerHandle::get_task_impl(detail::TaskInfo &&info) const {
    met_trace();
    return m_scheduler.get_task_impl(std::move(info));
  }

  detail::RsrcNode *LinearSchedulerHandle::add_rsrc_impl(detail::RsrcInfo &&info) {
    met_trace();
    return m_scheduler.add_rsrc_impl(std::move(info));
  }

  detail::RsrcNode *LinearSchedulerHandle::get_rsrc_impl(detail::RsrcInfo &&info) const {
    met_trace();

    // Find relevant task resources key/value iterator
    auto task_it = m_scheduler.m_rsrc_registry.find(info.task_key);
    guard(task_it != m_scheduler.m_rsrc_registry.end(), nullptr);
    
    // Find relevant resource key/value iterator
    auto rsrc_it = task_it->second.find(info.rsrc_key);
    guard(rsrc_it != task_it->second.end(), nullptr);

    return rsrc_it->second.get();
  }

  void LinearSchedulerHandle::rem_rsrc_impl(detail::RsrcInfo &&info) {
    met_trace();
    m_scheduler.rem_rsrc_impl(std::move(info));
  }
} // namespace met