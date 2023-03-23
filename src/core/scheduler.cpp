#include <metameric/core/scheduler.hpp>
#include <metameric/core/detail/trace.hpp>
#include <algorithm>
#include <execution>
#include <list>
#include <ranges>
#include <fmt/core.h>

namespace met {
  detail::TaskBase *LinearScheduler::add_task_impl(TaskInfo &&info) {
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
      m_task_order.emplace_back(task_key);
    } else {
      // Parent task key provided; Find end of range of said parent's other subtasks
      auto it = std::ranges::find_if(std::views::reverse(m_task_order), 
        [&](const auto &s) { return s.starts_with(info.prnt_key); }).base();
      m_task_order.emplace(it, task_key);
    }
    
    // Update task registries
    for (auto &info : handle.rem_task_info) rem_task_impl(std::move(info));
    for (auto &info : handle.add_task_info) add_task_impl(std::move(info));

    return it->second.get();
  }

  detail::TaskBase *LinearScheduler::get_task_impl(TaskInfo &&info) const {
    met_trace();
    
    // Final task key is parent_key.child_key
    std::string task_key = info.prnt_key.empty() 
                         ? info.task_key
                         : fmt::format("{}.{}", info.prnt_key, info.task_key);

    auto it = m_task_registry.find(task_key);
    guard(it != m_task_registry.end(), nullptr);

    return it->second.get();
  }

  void LinearScheduler::rem_task_impl(TaskInfo &&info) {
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
      for (auto &info : handle.rem_task_info) rem_task_impl(std::move(info));
      for (auto &info : handle.add_task_info) add_task_impl(std::move(info));
    }
  }

  detail::RsrcBase *LinearScheduler::add_rsrc_impl(RsrcInfo &&info) {
    met_trace();
    
    // Default to global_key if no task key is provided
    if (info.task_key.empty())
      info.task_key = global_key;
    
    // Insert s.t. resource registry is created if nonexistent
    auto [it, _] = m_rsrc_registry[info.task_key].insert_or_assign(info.rsrc_key, std::move(info.ptr));
    return it->second.get();
  }

  detail::RsrcBase *LinearScheduler::get_rsrc_impl(RsrcInfo &&info) const {
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

  void LinearScheduler::rem_rsrc_impl(RsrcInfo &&info) {
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

  void LinearScheduler::run_clear_state_impl() {
    met_trace();
    /* std::for_each(std::execution::par_unseq, range_iter(m_rsrc_registry), [](auto &pair) {
      for (auto &[_, node_ptr] : pair.second)
        node_ptr->clear_modify();
    }); */
  }

  void LinearScheduler::run_schedule_impl() {
    using Flags = LinearSchedulerHandle::HandleReturnFlags;

    met_trace();

    // Store task updates
    std::list<TaskInfo> add_task_info;
    std::list<TaskInfo> rem_task_info;
    Flags               flags = Flags::eNone;

    // Run all current tasks in copy of vector stored order
    for (const auto &task_key : m_task_order) {
      auto task = m_task_registry.at(task_key);

      // Reset cache state on task's owned resources
      if (auto it = m_rsrc_registry.find(task_key); it != m_rsrc_registry.end())
        std::ranges::for_each(it->second, [](auto &pair) { pair.second->set_mutated(false); });

      // Parse task info object by consuming task::eval_state() and task::eval()
      LinearSchedulerHandle handle(*this, task_key);
      guard_continue(task->eval_state(handle));
      task->eval(handle);

      // Process signal flags; clear tasks/resources if requested
      flags |= handle.return_flags;
      if (has_flag(flags, Flags::eClearTasks)) clear();
      if (has_flag(flags, Flags::eClearAll))   clear(false);
      
      // Defer task updates until current task is complete
      for (auto &info : handle.rem_task_info) rem_task_impl(std::move(info));
      for (auto &info : handle.add_task_info) add_task_impl(std::move(info));

      // Signal flag received; halt current run
      guard_break(!static_cast<uint>(flags)); 
    }

    // Process signal flag; rebuild schedule if requested
    if (has_flag(flags, Flags::eBuild)) build();
  }
  
  void LinearScheduler::clear(bool preserve_global) {
    met_trace();
    if (preserve_global) {
      std::list<std::string> task_order_copy(m_task_order);
      std::ranges::for_each(task_order_copy, [&](const auto &key) { rem_task_impl({ .task_key = key }); });
    } else {
      m_rsrc_registry.clear();
      m_task_registry.clear();
      m_task_order.clear();
    }
  }

  void LinearSchedulerHandle::build() {
    met_trace();
    return_flags |= HandleReturnFlags::eBuild;
  }

  void LinearSchedulerHandle::clear(bool preserve_global) {
    met_trace();
    return_flags |= (preserve_global ? HandleReturnFlags::eClearTasks : HandleReturnFlags::eClearAll);
  }

  detail::TaskBase *LinearSchedulerHandle::add_task_impl(TaskInfo &&info) {
    met_trace();
    add_task_info.emplace_back(std::move(info));
    return nullptr; // Task add is deferred to end of run, so cannot return
  }

  void LinearSchedulerHandle::rem_task_impl(TaskInfo &&info) {
    met_trace();
    rem_task_info.emplace_back(std::move(info));
  }

  detail::TaskBase *LinearSchedulerHandle::get_task_impl(TaskInfo &&info) const {
    met_trace();
    return m_scheduler.get_task_impl(std::move(info));
  }

  detail::RsrcBase *LinearSchedulerHandle::add_rsrc_impl(RsrcInfo &&info) {
    met_trace();
    return m_scheduler.add_rsrc_impl(std::move(info));
  }

  detail::RsrcBase *LinearSchedulerHandle::get_rsrc_impl(RsrcInfo &&info) const {
    met_trace();

    // Find relevant task resources key/value iterator
    auto task_it = m_scheduler.m_rsrc_registry.find(info.task_key);
    guard(task_it != m_scheduler.m_rsrc_registry.end(), nullptr);
    
    // Find relevant resource key/value iterator
    auto rsrc_it = task_it->second.find(info.rsrc_key);
    guard(rsrc_it != task_it->second.end(), nullptr);

    return rsrc_it->second.get();
  }

  void LinearSchedulerHandle::rem_rsrc_impl(RsrcInfo &&info) {
    met_trace();
    m_scheduler.rem_rsrc_impl(std::move(info));
  }

  void MaskedSchedulerHandle::build() {
    met_trace();
    m_masked_handle.build();
  }

  void MaskedSchedulerHandle::clear(bool preserve_global) {
    met_trace();
    m_masked_handle.clear(preserve_global);
  }
  
  detail::TaskBase *MaskedSchedulerHandle::add_task_impl(TaskInfo &&info) {
    met_trace();
    return m_masked_handle.add_task_impl(std::move(info));
  }

  void MaskedSchedulerHandle::rem_task_impl(TaskInfo &&info) {
    met_trace();
    m_masked_handle.rem_task_impl(std::move(info));
  }

  detail::TaskBase *MaskedSchedulerHandle::get_task_impl(TaskInfo &&info) const {
    met_trace();  
    return m_masked_handle.get_task_impl(std::move(info));
  }

  detail::RsrcBase *MaskedSchedulerHandle::add_rsrc_impl(RsrcInfo &&info) {
    met_trace();
    return m_masked_handle.add_rsrc_impl(std::move(info));
  }

  detail::RsrcBase *MaskedSchedulerHandle::get_rsrc_impl(RsrcInfo &&info) const {
    met_trace();
    return m_masked_handle.get_rsrc_impl(std::move(info));
  }

  void MaskedSchedulerHandle::rem_rsrc_impl(RsrcInfo &&info) {
    met_trace();
    m_masked_handle.rem_rsrc_impl(std::move(info));
  }
} // namespace met