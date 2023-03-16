#include <metameric/core/scheduler.hpp>
#include <metameric/core/detail/trace.hpp>
#include <algorithm>
#include <list>
#include <fmt/core.h>

namespace met {
  void LinearScheduler::add_task_impl(detail::AddTaskInfo &&info) {
    met_trace();

    // Parse task info object by consuming task
    detail::TaskInfo init(
      m_rsrc_registry, 
      m_task_registry, 
      info.task_key, 
      info.task, 
      detail::TaskInfoUsage::eInit
    );

    // Move task into registry
    if (info.prev_key.empty()) {
      // No previous task key provided; insert at end of list
      m_task_registry.emplace(std::pair { info.task_key, std::move(info.task) });
      m_task_order.emplace_back(info.task_key);
    } else {
      // Previous task key provided insert after said task
      auto it = std::ranges::find(m_task_order, info.prev_key);
      m_task_registry.emplace(std::pair { info.task_key, std::move(info.task) });
      m_task_order.emplace(it == m_task_order.end() ? it : it + 1, info.task_key);
    }
    
    // Update task/resource registries
    for (auto &info : init.add_rsrc_info) add_rsrc_impl(std::move(info));
    for (auto &info : init.rem_rsrc_info) rem_rsrc_impl(std::move(info));
    for (auto &info : init.rem_task_info) rem_task_impl(std::move(info));
    for (auto &info : init.add_task_info) add_task_impl(std::move(info));
  }

  void LinearScheduler::rem_task_impl(detail::RemTaskInfo &&info) {
    met_trace();

    // Check existence of specified task
    auto it = std::ranges::find(m_task_order, info.task_key);
    guard(it != m_task_order.end() && m_task_registry.contains(info.task_key));

    // Parse task info object by consuming task
    detail::TaskInfo dstr(
      m_rsrc_registry, 
      m_task_registry, 
      info.task_key, 
      m_task_registry.at(info.task_key), 
      detail::TaskInfoUsage::eDstr
    );
    
    // Update registries; remove task and resources
    m_task_registry.erase(info.task_key);
    m_rsrc_registry.erase(info.task_key);
    m_task_order.erase(it);

    // Update task registry; add/remove subtasks
    // Skip update of resource registry; it got deleted anyways
    for (auto &info : dstr.rem_task_info) rem_task_impl(std::move(info));
    for (auto &info : dstr.add_task_info) add_task_impl(std::move(info));
  }

  detail::RsrcNode LinearScheduler::add_rsrc_impl(detail::AddRsrcInfo &&info) {
    met_trace();
    
    // Default to global_key if no task key is provided
    if (info.task_key.empty())
      info.task_key = global_key;
    
    // Insert s.t. resource registry is created if nonexistent
    auto [it, _] = m_rsrc_registry[info.task_key].emplace(info.rsrc_key, std::move(info.rsrc));
    return it->second;
  }

  detail::RsrcNode LinearScheduler::get_rsrc_impl(detail::GetRsrcInfo &&info) {
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

    return rsrc_it->second;
  }

  void LinearScheduler::rem_rsrc_impl(detail::RemRsrcInfo &&info) {
    met_trace();

    // Default to global_key if no task key is provided
    if (info.task_key.empty())
      info.task_key = global_key;
    
    // Given existence of the specified task, erase specified resource
    if (auto it = m_rsrc_registry.find(info.task_key); it != m_rsrc_registry.end())
      it->second.erase(info.rsrc_key);
  }

  void LinearScheduler::build() {
    met_trace();

    // ...
  }

  void LinearScheduler::run() {
    met_trace();

    std::list<detail::AddTaskInfo> add_task_info;
    std::list<detail::RemTaskInfo> rem_task_info;
    std::list<detail::AddRsrcInfo> add_rsrc_info;
    std::list<detail::RemRsrcInfo> rem_rsrc_info;

    detail::TaskSignalFlags signal_flags = detail::TaskSignalFlags::eNone;

    // Run all tasks in vector stored order
    for (const auto &task_key : m_task_order) {
      // Parse task info object by consuming task::eval()
      detail::TaskInfo eval(
        m_rsrc_registry, 
        m_task_registry, 
        task_key, 
        m_task_registry.at(task_key), 
        detail::TaskInfoUsage::eEval
      );

      // Defer added/removed task/resource update until after run is complete
      rem_rsrc_info.splice(rem_rsrc_info.end(), eval.rem_rsrc_info);
      add_rsrc_info.splice(add_rsrc_info.end(), eval.add_rsrc_info);
      rem_task_info.splice(rem_task_info.end(), eval.rem_task_info);
      add_task_info.splice(add_task_info.end(), eval.add_task_info);

      // Signal flag received; should kill loop
      signal_flags |= eval.signal_flags; 
      if (static_cast<uint>(signal_flags)) { break; }
    }

    // Process signal flags; clear existing tasks/resources
    if (detail::has_flag(signal_flags, detail::TaskSignalFlags::eClearTasks)) { clear_tasks(); }
    if (detail::has_flag(signal_flags, detail::TaskSignalFlags::eClearAll))   { clear_all(); }

    // Update task/resource registry; add/remove
    for (auto &info : rem_rsrc_info) rem_rsrc_impl(std::move(info));
    for (auto &info : add_rsrc_info) add_rsrc_impl(std::move(info));
    for (auto &info : rem_task_info) rem_task_impl(std::move(info));
    for (auto &info : add_task_info) add_task_impl(std::move(info));
  }
  
  // Deregister all tasks safely; some tasks remove their own subtasks upon destruction
  // making iteration of m_task_order tricky. Ignore resources, these are removed automatically 
  // for specific removed tasks
  void LinearScheduler::clear_tasks() {
    met_trace();
    std::vector<std::string> task_order_copy(m_task_order);
    std::ranges::for_each(task_order_copy, [&](const auto &key) { remove_task(key); });
  }

  void LinearScheduler::clear_global() {
    met_trace();
    m_rsrc_registry.erase(global_key);
  }

  void LinearScheduler::clear_all() {
    met_trace();
    m_rsrc_registry.clear();
    m_task_registry.clear();
    m_task_order.clear();
  }
} // namespace met