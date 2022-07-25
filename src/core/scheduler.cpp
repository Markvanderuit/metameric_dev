#include <metameric/core/scheduler.hpp>
#include <algorithm>
#include <list>
#include <fmt/core.h>

namespace met {
  void LinearScheduler::register_task(const KeyType &prev, TaskType &&task) {
    // Parse task info object by consuming task
    detail::TaskInitInfo info(_resource_registry, *task.get());
    
    // Update resource registry; add/remove task resources
    auto &local_registry = _resource_registry[task->name()];
    local_registry.merge(info.add_resource_registry);
    for (auto &key : info.remove_resource_registry) { local_registry.erase(key); }
      
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
    for (auto &[key, task] : info.add_task_registry) { register_task(key, std::move(task)); }
    for (auto &key : info.remove_task_registry)      { remove_task(key); }
  }
  
  void LinearScheduler::deregister_task(TaskType &task) {
    // Parse task info object by consuming task
    detail::TaskDstrInfo info(_resource_registry, *task.get());

    // Update resource registry; remove task resources
    _resource_registry.erase(task->name());

    // Update task registry; add+register/remove subtasks
    for (auto &[key, task] : info.add_task_registry) { register_task(key, std::move(task)); }
    for (auto &key : info.remove_task_registry)      { remove_task(key); }
  }

  void LinearScheduler::run() {
    std::list<std::pair<KeyType, TaskType>> add_task_registry;
    std::list<KeyType>                      remove_task_registry;
    detail::TaskSignalFlags                 signal_flags = detail::TaskSignalFlags::eNone;

    // Run all tasks in vector inserted order
    for (auto &task : _task_registry) {
      // Parse task info object by consuming task::eval()
      detail::TaskEvalInfo info(_resource_registry, *task.get());

      signal_flags |= info.signal_flags; // Process task/resource editing signals later

      // Process added/removed resources **immediately** after task execution
      if (!info.add_resource_registry.empty() || !info.remove_resource_registry.empty()) {
        auto &local_registry = _resource_registry[task->name()];
        local_registry.merge(std::move(info.add_resource_registry));
        for (auto &key : info.remove_resource_registry) { local_registry.erase(key); }
      }

      // Defer added/removed task update until after all tasks have completed
      if (!info.add_task_registry.empty() || !info.remove_task_registry.empty()) {
        add_task_registry.splice(add_task_registry.end(), info.add_task_registry);
        remove_task_registry.splice(remove_task_registry.end(), info.remove_task_registry);
      }
    }

    // Process signal flags; clear tasks/resources
    if (detail::has_flag(signal_flags, detail::TaskSignalFlags::eClearTasks)) { clear_tasks(); }
    if (detail::has_flag(signal_flags, detail::TaskSignalFlags::eClearAll))   { clear_all(); }

    // Update task registry; add+register/remove subtasks
    for (auto &[key, task] : add_task_registry) { register_task(key, std::move(task)); }
    for (auto &key : remove_task_registry)      { remove_task(key); }
  }
  
  void LinearScheduler::remove_task(const KeyType &key) {
    std::erase_if(_task_registry, [&](auto &task) { 
      if (task->name() == key) {
        deregister_task(task);
        return true;
      }
      return false;
    });
  }
  
  void LinearScheduler::erase_resource(const KeyType &key) {
    _resource_registry[global_key].erase(key);
  }

  void LinearScheduler::clear_tasks() {
    std::erase_if(_resource_registry, [&](const auto &p) {
      return p.first != global_key;
    });
    _task_registry.clear();
  }

  void LinearScheduler::clear_all() {
    _resource_registry.clear();
    _task_registry.clear();
  }
} // namespace met