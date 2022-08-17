#include <metameric/core/scheduler.hpp>
#include <metameric/core/detail/trace.hpp>
#include <algorithm>
#include <list>
#include <fmt/core.h>

namespace met {
  void LinearScheduler::register_task(const KeyType &prev, TaskType &&task) {
    met_trace();

    // Parse task info object by consuming task
    detail::TaskInitInfo info(_rsrc_registry, _task_registry, *task.get());
    
    // Update resource registry; add/remove task resources
    auto &local_registry = _rsrc_registry[task->name()];
    local_registry.merge(info.add_rsrc_registry);
    for (auto &key : info.rem_rsrc_registry) { local_registry.erase(key); }
      
    // Move task into registry
    if (prev.empty()) {
      // No previous task key provided; insert at end of list
      _task_registry.emplace_back(std::move(task));
    } else {
      // Previous task key provided insert after said task
      auto i = std::ranges::find_if(_task_registry, [prev](const auto &t) { return t->name() == prev; });
      _task_registry.emplace(i == _task_registry.end() ? i : i + 1, std::move(task));
    }

    // Update task registry; add+register/remove subtasks
    for (auto &key         : info.rem_task_registry) { deregister_task(key); }
    for (auto &[key, task] : info.add_task_registry) { register_task(key, std::move(task)); }
  }
  
  void LinearScheduler::deregister_task(const KeyType &key) {
    met_trace();

    // Find existing task
    auto i = std::ranges::find_if(_task_registry, [&](auto &task) { return task->name() == key; });
    guard(i != _task_registry.end());
    auto &task = *i;

    // Parse task info object by consuming task
    detail::TaskDstrInfo info(_rsrc_registry, _task_registry, *task.get());
    
    // Update registries; remove task and resources
    _rsrc_registry.erase(task->name());
    _task_registry.erase(i);

    // Update task registry; add+register/remove subtasks
    for (auto &key         : info.rem_task_registry) { deregister_task(key); }
    for (auto &[key, task] : info.add_task_registry) { register_task(key, std::move(task)); }
  }

  void LinearScheduler::run() {
    met_trace();

    std::list<std::pair<KeyType, TaskType>> add_task_registry;
    std::list<KeyType>                      rem_task_registry;
    detail::TaskSignalFlags                 signal_flags = detail::TaskSignalFlags::eNone;

    // Run all tasks in vector inserted order
    for (auto &task : _task_registry) {
      // Parse task info object by consuming task::eval()
      detail::TaskEvalInfo info(_rsrc_registry, _task_registry, *task.get());

      // Process added/removed resources **immediately** after task execution
      if (!info.add_rsrc_registry.empty() || !info.rem_rsrc_registry.empty()) {
        auto &local_registry = _rsrc_registry[task->name()];
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
  
  void LinearScheduler::remove_task(const KeyType &key) {
    met_trace();
    deregister_task(key);
  }
  
  void LinearScheduler::remove_resource(const KeyType &key) {
    met_trace();
    _rsrc_registry[global_key].erase(key);
  }

  void LinearScheduler::clear_tasks() {
    met_trace();
    std::erase_if(_rsrc_registry, [&](const auto &p) { return p.first != global_key; });
    _task_registry.clear();
  }

  void LinearScheduler::clear_global() {
    met_trace();
    _rsrc_registry.erase(global_key);
  }

  void LinearScheduler::clear_all() {
    met_trace();
    _rsrc_registry.clear();
    _task_registry.clear();
  }
} // namespace met