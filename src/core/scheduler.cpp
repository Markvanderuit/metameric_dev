#include <metameric/core/scheduler.hpp>
#include <metameric/core/detail/trace.hpp>
#include <algorithm>
#include <list>
#include <fmt/core.h>

namespace met {
  void LinearScheduler::register_task(const std::string &prev, TaskType &&task) {
    met_trace();

    // Parse task info object by consuming task
    detail::TaskInfo info(m_rsrc_registry, m_task_registry, *task.get(), detail::TaskInfoUsage::eInit);
    
    // Update resource registry; add/remove task resources
    auto &local_registry = m_rsrc_registry[task->name()];
    local_registry.merge(info.add_rsrc_registry);
    for (auto &key : info.rem_rsrc_registry) { local_registry.erase(key); }
      
    // Move task into registry
    if (prev.empty()) {
      // No previous task key provided; insert at end of list
      m_task_registry.emplace_back(std::move(task));
    } else {
      // Previous task key provided insert after said task
      auto i = std::ranges::find_if(m_task_registry, [prev](const auto &t) { return t->name() == prev; });
      m_task_registry.emplace(i == m_task_registry.end() ? i : i + 1, std::move(task));
    }

    // Update task registry; add+register/remove subtasks
    for (auto &key         : info.rem_task_registry) { deregister_task(key); }
    for (auto &[key, task] : info.add_task_registry) { register_task(key, std::move(task)); }
  }
  
  void LinearScheduler::deregister_task(const std::string &key) {
    met_trace();

    // Find existing task
    auto i = std::ranges::find_if(m_task_registry, [&](auto &task) { return task->name() == key; });
    guard(i != m_task_registry.end());
    auto &task = *i;

    // Parse task info object by consuming task
    detail::TaskInfo info(m_rsrc_registry, m_task_registry, *task.get(), detail::TaskInfoUsage::eDstr);
    
    // Update registries; remove task and resources
    m_rsrc_registry.erase(task->name());
    m_task_registry.erase(i);

    // Update task registry; add+register/remove subtasks
    for (auto &key         : info.rem_task_registry) { deregister_task(key); }
    for (auto &[key, task] : info.add_task_registry) { register_task(key, std::move(task)); }
  }

  void LinearScheduler::build() {
    met_trace();


  }

  void LinearScheduler::run() {
    met_trace();

    std::list<std::pair<std::string, TaskType>> add_task_registry;
    std::list<std::string>                      rem_task_registry;
    detail::TaskSignalFlags                      signal_flags = detail::TaskSignalFlags::eNone;

    // Run all tasks in vector inserted order
    for (auto &task : m_task_registry) {
      // Parse task info object by consuming task::eval()
      detail::TaskInfo info(m_rsrc_registry, m_task_registry, *task.get(), detail::TaskInfoUsage::eEval);

      // Process added/removed resources **immediately** after task execution
      if (!info.add_rsrc_registry.empty() || !info.rem_rsrc_registry.empty()) {
        auto &local_registry = m_rsrc_registry[task->name()];
        local_registry.merge(std::move(info.add_rsrc_registry));
        for (auto &key : info.rem_rsrc_registry) { local_registry.erase(key); }
      }

      // Defer added/removed task update until after all tasks have completed
      if (!info.add_task_registry.empty() || !info.rem_task_registry.empty()) {
        rem_task_registry.splice(rem_task_registry.end(), info.rem_task_registry);
        add_task_registry.splice(add_task_registry.end(), info.add_task_registry);
      }

      // Signal flag received; should kill loop
      signal_flags |= info.signal_flags; 
      if ((uint) signal_flags) { break; }
    }

    // Process signal flags; clear existing tasks/resources
    if (detail::has_flag(signal_flags, detail::TaskSignalFlags::eClearTasks)) { clear_tasks(); }
    if (detail::has_flag(signal_flags, detail::TaskSignalFlags::eClearAll))   { clear_all(); }

    // Update task registry; add+register/remove subtasks
    for (auto &key         : rem_task_registry) { deregister_task(key); }
    for (auto &[key, task] : add_task_registry) { register_task(key, std::move(task)); }
  }
  
  void LinearScheduler::clear_tasks() {
    met_trace();

    // Gather key names from all tasks
    std::vector<std::string> keys(m_task_registry.size());
    std::ranges::transform(m_task_registry, keys.begin(), [](const auto &task) { return task->name(); });

    // Deregister all tasks safely; some tasks remove their own subtasks upon destruction
    for (auto &key : keys) 
      deregister_task(key);
    
    // Remove all resources not marked with global_key, though there should be none due to deregister_task
    std::erase_if(m_rsrc_registry, [&](const auto &pair) { return pair.first != global_key; });
  }

  void LinearScheduler::clear_global() {
    met_trace();
    m_rsrc_registry.erase(global_key);
  }

  void LinearScheduler::clear_all() {
    met_trace();
    m_rsrc_registry.clear();
    m_task_registry.clear();
  }
} // namespace met