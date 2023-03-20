#include <metameric/core/scheduler.hpp>
#include <metameric/core/detail/trace.hpp>
#include <algorithm>
#include <list>
#include <ranges>
#include <fmt/core.h>

namespace met {
  void LinearScheduler::add_task_impl(AddTaskInfo &&info) {
    met_trace();

    // Final task key is parent_key.child_key
    std::string task_key = info.prnt_key.empty() 
                         ? info.task_key
                         : fmt::format("{}.{}", info.prnt_key, info.task_key);

    // Parse task info object by consuming task
    LinearSchedulerHandle handle(*this, task_key);
    info.task->init(handle);

    // Move task into registry
    m_task_registry.emplace(std::pair { task_key, std::move(info.task) });
    if (info.prnt_key.empty()) {
      // No previous task key provided; insert at end of list
      m_task_order.emplace_back(task_key);
    } else {
      // Parent task key provided; Find end of range of said parent's other subtasks
      auto filt = m_task_order 
                | std::views::filter([&](const auto &s) { return s.starts_with(info.prnt_key); });
      debug::check_expr_dbg(!filt.empty(), "Added subtask to nonexistent parent");
                
      // Insert subtask at end of range
      auto it = m_task_order.begin() + std::distance(m_task_order.data(), &filt.back());
      m_task_order.emplace(it == m_task_order.end() ? it : it + 1, task_key);
    }
    
    // Update task/resource registries
    for (auto &info : handle.rem_rsrc_info) rem_rsrc_impl(std::move(info));
    for (auto &info : handle.add_rsrc_info) add_rsrc_impl(std::move(info));
    for (auto &info : handle.rem_task_info) rem_task_impl(std::move(info));
    for (auto &info : handle.add_task_info) add_task_impl(std::move(info));
  }

  void LinearScheduler::rem_task_impl(RemTaskInfo &&info) {
    met_trace();

    // Final task key is parent_key.child_key
    std::string task_key = info.prnt_key.empty() 
                         ? info.task_key
                         : fmt::format("{}.{}", info.prnt_key, info.task_key);

    // Find range of tasks with task_key as key, or task_key as parent
    auto filt = m_task_order 
              | std::views::filter([&](const auto &s) { return s.starts_with(task_key); })
              | std::views::reverse;
    guard(!filt.empty());

    // Iterate removable subtasks in reverse order
    std::vector<std::string> remove_range(range_iter(filt));
    for (const auto &remove_key : remove_range) {
      auto it = std::ranges::find(m_task_order, remove_key);
      guard(it != m_task_order.end() && m_task_registry.contains(remove_key));

      // Create handle object and let task run
      LinearSchedulerHandle handle(*this, remove_key);
      m_task_registry.at(remove_key)->dstr(handle);
  
      // Update registries; remove task and resources
      m_task_registry.erase(remove_key);
      m_rsrc_registry.erase(remove_key);
      m_task_order.erase(it);

      // Update task registry; add/remove other specified subtasks
      // Skip update of resource registry; it got deleted anyways
      for (auto &info : handle.rem_task_info) rem_task_impl(std::move(info));
      for (auto &info : handle.add_task_info) add_task_impl(std::move(info));
    }
  }

  detail::RsrcBase *LinearScheduler::add_rsrc_impl(AddRsrcInfo &&info) {
    met_trace();
    
    // Default to global_key if no task key is provided
    if (info.task_key.empty())
      info.task_key = global_key;
    
    // Insert s.t. resource registry is created if nonexistent
    auto [it, _] = m_rsrc_registry[info.task_key].emplace(info.rsrc_key, std::move(info.rsrc));
    return it->second.get();
  }

  detail::RsrcBase *LinearScheduler::get_rsrc_impl(GetRsrcInfo &&info) {
    met_trace();
    
    // Default to global_key if no task key is provided
    if (info.task_key.empty())
      info.task_key = global_key;

    // Find relevant task resources key/value iterator
    auto task_it = m_rsrc_registry.find(info.task_key);
    guard(task_it != m_rsrc_registry.end(), nullptr);
    
    // Find relevant resource key/value iterator
    auto rsrc_it = task_it->second.find(info.rsrc_key);
    guard(rsrc_it != task_it->second.end(), nullptr);

    return rsrc_it->second.get();
  }

  void LinearScheduler::rem_rsrc_impl(RemRsrcInfo &&info) {
    met_trace();

    // Default to global_key if no task key is provided
    if (info.task_key.empty())
      info.task_key = global_key;
    
    // Given existence of the specified task, erase specified resource
    if (auto it = m_rsrc_registry.find(info.task_key); it != m_rsrc_registry.end()) {
      fmt::print("rem_rsrc_impl {} - {}\n", info.task_key, info.rsrc_key);
      it->second.erase(info.rsrc_key);
    }
  }

  void LinearScheduler::build() {
    met_trace();

    // ...
  }

  void LinearScheduler::run() {
    met_trace();

    std::list<AddTaskInfo> add_task_info;
    std::list<RemTaskInfo> rem_task_info;
    std::list<AddRsrcInfo> add_rsrc_info;
    std::list<RemRsrcInfo> rem_rsrc_info;

    using Flags = LinearSchedulerHandle::ClearFlags;
    Flags clear_flags = Flags::eNone;

    // Run all tasks in vector stored order
    for (const auto &task_key : m_task_order) {
      // Parse task info object by consuming task::eval()
      LinearSchedulerHandle handle(*this, task_key);
      m_task_registry.at(task_key)->eval(handle);

      // Defer task/resource updates until current run is complete
      rem_rsrc_info.splice(rem_rsrc_info.end(), handle.rem_rsrc_info);
      add_rsrc_info.splice(add_rsrc_info.end(), handle.add_rsrc_info);
      rem_task_info.splice(rem_task_info.end(), handle.rem_task_info);
      add_task_info.splice(add_task_info.end(), handle.add_task_info);

      // Signal flag received; halt current run
      clear_flags |= handle.clear_flags; 
      if (static_cast<uint>(clear_flags)) { break; }
    }

    // Process signal flags; clear existing/all tasks/resources if requested
    if (has_flag(clear_flags, Flags::eClearTasks)) clear();
    if (has_flag(clear_flags, Flags::eClearAll))   clear(false);
    if (has_flag(clear_flags, Flags::eBuild))      build();

    // Process task/resource updates
    for (auto &info : rem_rsrc_info) rem_rsrc_impl(std::move(info));
    for (auto &info : add_rsrc_info) add_rsrc_impl(std::move(info));
    for (auto &info : rem_task_info) rem_task_impl(std::move(info));
    for (auto &info : add_task_info) add_task_impl(std::move(info));
  }
  
  void LinearScheduler::clear(bool preserve_global) {
    met_trace();
    if (preserve_global) {
      std::vector<std::string> task_order_copy(m_task_order);
      std::ranges::for_each(task_order_copy, [&](const auto &key) { remove_task(key); });
    } else {
      m_rsrc_registry.clear();
      m_task_registry.clear();
      m_task_order.clear();
    }
  }

  void LinearSchedulerHandle::build() {
    met_trace();
    clear_flags |= ClearFlags::eBuild;
  }

  void LinearSchedulerHandle::clear(bool preserve_global) {
    met_trace();
    clear_flags |= (preserve_global ? ClearFlags::eClearTasks : ClearFlags::eClearAll);
  }

  void LinearSchedulerHandle::add_task_impl(AddTaskInfo &&info) {
    met_trace();
    add_task_info.emplace_back(std::move(info));
  }

  void LinearSchedulerHandle::rem_task_impl(RemTaskInfo &&info) {
    met_trace();
    rem_task_info.emplace_back(std::move(info));
  }

  detail::RsrcBase *LinearSchedulerHandle::add_rsrc_impl(AddRsrcInfo &&info) {
    met_trace();
    return add_rsrc_info.emplace_back(std::move(info)).rsrc.get();
  }

  detail::RsrcBase *LinearSchedulerHandle::get_rsrc_impl(GetRsrcInfo &&info) {
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
  }
} // namespace met